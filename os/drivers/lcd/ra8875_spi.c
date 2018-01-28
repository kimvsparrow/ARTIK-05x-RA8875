/****************************************************************************
 *
 * Copyright 2017 Kim Sparrow All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <tinyara/config.h>

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/lcd/lcd.h>
#include <tinyara/lcd/ra8875.h>
#include <tinyara/spi/spi.h>


static FAR struct spi_dev_s *init_ra8875_spi(void);

static void spi_write_reg(FAR struct ra8875_lcd_s *dev, uint8_t regnum, uint8_t data);
static void spi_write_reg16(FAR struct ra8875_lcd_s *dev, uint8_t regnum, uint16_t data);
static uint8_t spi_read_reg(FAR struct ra8875_lcd_s *dev, uint8_t regnum);
static uint8_t spi_read_status(FAR struct ra8875_lcd_s *dev);
static void spi_pwrite_prepare(FAR struct ra8875_lcd_s *dev, uint8_t regnum);
static void spi_pwrite_data8(FAR struct ra8875_lcd_s *dev, uint8_t data);
static void spi_pwrite_data16(FAR struct ra8875_lcd_s *dev, uint16_t data);
static void spi_pwrite_finish(FAR struct ra8875_lcd_s *dev);
static void spi_pread_prepare(FAR struct ra8875_lcd_s *dev, uint8_t regnum);
static uint16_t spi_pread_data16(FAR struct ra8875_lcd_s *dev);
static void spi_pread_finish(FAR struct ra8875_lcd_s *dev);

static void ra8875_spi_command_write(uint8_t regnum);
static void ra8875_spi_data_write(uint8_t data);
static void ra8875_spi_data_write16(uint16_t data);
static uint8_t ra8875_spi_data_read(void);
static uint16_t ra8875_spi_data_read16(void);
static uint8_t ra8875_spi_status_read(void);

#define SPI_START_SPEED 3000000
#define SPI_WRITE_SPEED 20000000
#define SPI_READ_SPEED 10000000

#define CYCLE_START() do { SPI_SELECT(spi_device, 0, TRUE); } while (0)
#define CYCLE_END() do { SPI_SELECT(spi_device, 0, FALSE); } while (0)

#define CMD_WRITE (2 << 6)
#define STAT_READ (3 << 6)
#define DATA_WRITE (0 << 6)
#define DATA_READ (1 << 6)

static uint8_t cmd_write_buffer[2] = {CMD_WRITE, 0};
static uint8_t stat_read_buffer[2] = {STAT_READ, 0};
static uint8_t data_write_buffer[3] = {DATA_WRITE, 0, 0};
static uint8_t data_read_buffer[3] = {DATA_READ, 0, 0};

#ifdef CONFIG_LCD_RA8875_PWRITE_BUFFER_SIZE
#define USE_PWRITE_BUFFER
#define WRITE_BUFFER_SIZE CONFIG_LCD_RA8875_PWRITE_BUFFER_SIZE
static uint8_t pwrite_buffer[WRITE_BUFFER_SIZE] __attribute__ ((aligned (2)));
static size_t pwrite_buffer_index;
#endif

static struct ra8875_lcd_s ra8875_spi = {
    .write_reg = spi_write_reg,
    .write_reg16 = spi_write_reg16,
    .read_reg = spi_read_reg,
    .read_status = spi_read_status,
    .pwrite_prepare = spi_pwrite_prepare,
    .pwrite_data8 = spi_pwrite_data8,
    .pwrite_data16 = spi_pwrite_data16,
    .pwrite_finish = spi_pwrite_finish,
    .pread_prepare = spi_pread_prepare,
    .pread_data16 = spi_pread_data16,
    .pread_finish = spi_pread_finish
};

FAR static struct spi_dev_s *spi_device;
FAR static struct lcd_dev_s *lcd_device;

int board_lcd_initialize(void) {
    spi_device = init_ra8875_spi();
    lcd_device = ra8875_lcdinitialize(&ra8875_spi);
    SPI_SETFREQUENCY(spi_device, SPI_READ_SPEED);
    return lcd_device != NULL;
}

FAR struct lcd_dev_s *board_lcd_getdev(int lcddev) {
    return lcd_device;
}

void board_lcd_uninitialize(void) {
	SPI_LOCK(spi_device, FALSE);
}

static FAR struct spi_dev_s *init_ra8875_spi(void) {
    struct spi_dev_s *spi;

    spi = up_spiinitialize(0);
	if (spi == NULL) {
		return NULL;
    }

	SPI_LOCK(spi, TRUE);
	SPI_SETMODE(spi, SPIDEV_MODE0);
	SPI_SETBITS(spi, 8);
	SPI_SETFREQUENCY(spi, SPI_START_SPEED);
    return spi;
}

static void spi_write_reg(FAR struct ra8875_lcd_s *dev, uint8_t regnum, uint8_t data) {
    ra8875_spi_command_write(regnum);
    ra8875_spi_data_write(data);
}

static void spi_write_reg16(FAR struct ra8875_lcd_s *dev, uint8_t regnum, uint16_t data) {
    ra8875_spi_command_write(regnum);
    ra8875_spi_data_write16(data);
}

static uint8_t spi_read_reg(FAR struct ra8875_lcd_s *dev, uint8_t regnum) {
    ra8875_spi_command_write(regnum);
    return ra8875_spi_data_read();
}

static uint8_t spi_read_status(FAR struct ra8875_lcd_s *dev) {
    return ra8875_spi_status_read();
}

static void spi_pwrite_prepare(FAR struct ra8875_lcd_s *dev, uint8_t regnum) {
    SPI_SETFREQUENCY(spi_device, SPI_WRITE_SPEED);
    ra8875_spi_command_write(regnum);
    CYCLE_START();
#ifdef USE_PWRITE_BUFFER
    pwrite_buffer_index = 0;
#endif
}

// XXX: speed can be improved by batching SPI data for large block writes... 16 bytes?
static void spi_pwrite_data8(FAR struct ra8875_lcd_s *dev, uint8_t data) {
#ifdef USE_PWRITE_BUFFER
    pwrite_buffer[pwrite_buffer_index++] = data;
    if (pwrite_buffer_index == WRITE_BUFFER_SIZE) {
        SPI_SNDBLOCK(spi_device, pwrite_buffer, WRITE_BUFFER_SIZE);
        pwrite_buffer_index = 0;
    }
#else
    ra8875_spi_data_write(data);
#endif
}

static void spi_pwrite_data16(FAR struct ra8875_lcd_s *dev, uint16_t data) {
#ifdef USE_PWRITE_BUFFER
    *(uint16_t*)(&pwrite_buffer[pwrite_buffer_index]) = data;
    pwrite_buffer_index += 2;
    if (pwrite_buffer_index == WRITE_BUFFER_SIZE) {
        SPI_SNDBLOCK(spi_device, pwrite_buffer, WRITE_BUFFER_SIZE);
        pwrite_buffer_index = 0;
    }
#else
    ra8875_spi_data_write16(data);
#endif
}

static void spi_pwrite_finish(FAR struct ra8875_lcd_s *dev) {
#ifdef USE_PWRITE_BUFFER
    if (pwrite_buffer_index != 0) {
        SPI_SNDBLOCK(spi_device, pwrite_buffer, pwrite_buffer_index);
    }
#endif
    CYCLE_END();
    SPI_SETFREQUENCY(spi_device, SPI_READ_SPEED);
}

static void spi_pread_prepare(FAR struct ra8875_lcd_s *dev, uint8_t regnum) {
    uint8_t read_block[2];

    ra8875_spi_command_write(regnum);
    CYCLE_START();
    SPI_EXCHANGE(spi_device, data_read_buffer, read_block, 2);
}

static uint16_t spi_pread_data16(FAR struct ra8875_lcd_s *dev) {
    uint8_t read_block[2];

    SPI_RECVBLOCK(spi_device, read_block, 2);
    return (read_block[1] << 8) | read_block[0];
}

static void spi_pread_finish(FAR struct ra8875_lcd_s *dev) {
    CYCLE_END();
}


static void ra8875_spi_command_write(uint8_t regnum) {
    cmd_write_buffer[1] = regnum;
    CYCLE_START();
    SPI_SNDBLOCK(spi_device, cmd_write_buffer, 2);
    CYCLE_END();
}

static void ra8875_spi_data_write(uint8_t data) {
    data_write_buffer[1] = data;
    CYCLE_START();
    SPI_SNDBLOCK(spi_device, data_write_buffer, 2);
    CYCLE_END();
}

static void ra8875_spi_data_write16(uint16_t data) {
    data_write_buffer[1] = data & 0xff;
    data_write_buffer[2] = data >> 8;
    CYCLE_START();
    SPI_SNDBLOCK(spi_device, data_write_buffer, 3);
    CYCLE_END();
}

static uint8_t ra8875_spi_data_read(void) {
    uint8_t read_block[2];
    CYCLE_START();
    SPI_EXCHANGE(spi_device, data_read_buffer, read_block, 2);
    CYCLE_END();
    return read_block[1];
}

static uint16_t ra8875_spi_data_read16(void) {
    uint8_t read_block[3];
    CYCLE_START();
    SPI_EXCHANGE(spi_device, data_read_buffer, read_block, 3);
    CYCLE_END();
    return (read_block[2] << 8) | read_block[1];
}

static uint8_t ra8875_spi_status_read(void) {
    uint8_t read_block[2];
    CYCLE_START();
    SPI_EXCHANGE(spi_device, stat_read_buffer, read_block, 2);
    CYCLE_END();
    return read_block[1];
}
