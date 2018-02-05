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
static uint8_t ra8875_spi_data_read(void);
static uint8_t ra8875_spi_status_read(void);

#define SPI_START_SPEED    1000000
#define SPI_WRITE_SPEED    10000000
#define SPI_WRITERUN_SPEED 20000000
#define SPI_READ_SPEED     6000000

#define CYCLE_START() do { SPI_SELECT(spi_device, 0, TRUE); } while (0)
#define CYCLE_END() do { SPI_SELECT(spi_device, 0, FALSE); } while (0)

#define CMD_WRITE (2 << 6)
#define STAT_READ (3 << 6)
#define DATA_WRITE (0 << 6)
#define DATA_READ (1 << 6)

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
    if (spi_device == NULL) {
        return -EIO;
    }
    lcd_device = ra8875_lcdinitialize(&ra8875_spi);
    if (lcd_device == NULL) {
        return -ENODEV;
    }
    SPI_SETFREQUENCY(spi_device, SPI_WRITE_SPEED);

    return OK;
}

FAR struct lcd_dev_s *board_lcd_getdev(int lcddev) {
    return lcd_device;
}

void board_lcd_uninitialize(void) {
	SPI_LOCK(spi_device, FALSE);
}

static FAR struct spi_dev_s *init_ra8875_spi(void) {
    struct spi_dev_s *spi;

    spi = up_spiinitialize(CONFIG_LCD_RA8875_SPI_4WIRE_BUS);
	if (spi == NULL) {
        lcderr("ERROR: Failed to initialize SPI for RA8875");
		return NULL;
    }

	SPI_LOCK(spi, TRUE);
	SPI_SETFREQUENCY(spi, SPI_START_SPEED);
	SPI_SETMODE(spi, SPIDEV_MODE0);
	SPI_SETBITS(spi, 8);
    return spi;
}

static void spi_write_reg(FAR struct ra8875_lcd_s *dev, uint8_t regnum, uint8_t data) {
    ra8875_spi_command_write(regnum);
    ra8875_spi_data_write(data);
}

static void spi_write_reg16(FAR struct ra8875_lcd_s *dev, uint8_t regnum, uint16_t data) {
    ra8875_spi_command_write(regnum);
    ra8875_spi_data_write(data & 0xff);
    ra8875_spi_command_write(regnum + 1);
    ra8875_spi_data_write(data >> 8);
}

static uint8_t spi_read_reg(FAR struct ra8875_lcd_s *dev, uint8_t regnum) {
	SPI_SETFREQUENCY(spi_device, SPI_READ_SPEED);
    ra8875_spi_command_write(regnum);
    return ra8875_spi_data_read();
	SPI_SETFREQUENCY(spi_device, SPI_WRITE_SPEED);
}

static uint8_t spi_read_status(FAR struct ra8875_lcd_s *dev) {
	SPI_SETFREQUENCY(spi_device, SPI_READ_SPEED);
    return ra8875_spi_status_read();
	SPI_SETFREQUENCY(spi_device, SPI_WRITE_SPEED);
}

static void spi_pwrite_prepare(FAR struct ra8875_lcd_s *dev, uint8_t regnum) {
    ra8875_spi_command_write(regnum);
    SPI_SETFREQUENCY(spi_device, SPI_WRITERUN_SPEED);
    CYCLE_START();
}

static void spi_pwrite_data8(FAR struct ra8875_lcd_s *dev, uint8_t data) {
    SPI_SNDBLOCK(spi_device, &data, 1);
}

static void spi_pwrite_data16(FAR struct ra8875_lcd_s *dev, uint16_t data) {
    uint8_t buffer[2] = {data & 0xff, data >> 8};

    SPI_SNDBLOCK(spi_device, buffer, 2);
}

static void spi_pwrite_finish(FAR struct ra8875_lcd_s *dev) {
    CYCLE_END();
    SPI_SETFREQUENCY(spi_device, SPI_WRITE_SPEED);
}

static void spi_pread_prepare(FAR struct ra8875_lcd_s *dev, uint8_t regnum) {
    uint8_t write_block[1] = { DATA_READ };
    uint8_t read_block[1];

    ra8875_spi_command_write(regnum);
	SPI_SETFREQUENCY(spi_device, SPI_READ_SPEED);
    CYCLE_START();
    SPI_EXCHANGE(spi_device, write_block, read_block, 1);
}

static uint16_t spi_pread_data16(FAR struct ra8875_lcd_s *dev) {
    uint8_t read_block[2];

    SPI_RECVBLOCK(spi_device, read_block, 2);
    return (read_block[1] << 8) | read_block[0];
}

static void spi_pread_finish(FAR struct ra8875_lcd_s *dev) {
    CYCLE_END();
	SPI_SETFREQUENCY(spi_device, SPI_WRITE_SPEED);
}

static void ra8875_spi_command_write(uint8_t regnum) {
    uint8_t write_block[2] = { CMD_WRITE, regnum };

    CYCLE_START();
    SPI_SNDBLOCK(spi_device, write_block, 2);
    CYCLE_END();
}

static void ra8875_spi_data_write(uint8_t data) {
    uint8_t write_block[2] = { DATA_WRITE, data };

    CYCLE_START();
    SPI_SNDBLOCK(spi_device, write_block, 2);
    CYCLE_END();
}

static uint8_t ra8875_spi_data_read(void) {
    uint8_t write_block[2] = { DATA_READ, 0 };
    uint8_t read_block[2];

    CYCLE_START();
    SPI_EXCHANGE(spi_device, write_block, read_block, 2);
    CYCLE_END();
    return read_block[1];
}

static uint8_t ra8875_spi_status_read(void) {
    uint8_t write_block[2] = { STAT_READ, 0 };
    uint8_t read_block[2];

    CYCLE_START();
    SPI_EXCHANGE(spi_device, write_block, read_block, 2);
    CYCLE_END();
    return read_block[1];
}
