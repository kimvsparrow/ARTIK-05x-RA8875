# TizenRT for ARTIK05x w/ RA8875

This project forks [TizenRT](https://github.com/SamsungARTIK/TizenRT) from [SamsungARTIK](https://github.com/SamsungARTIK), the ARTIK 05x-focused fork of [TizenRT 1.0](https://github.com/Samsung/TizenRT) from [SamsungOpenSource](https://github.com/Samsung).

TizenRT is a lightweight RTOS-based platform to support low-end IoT devices. It is largely based on [NuttX](https://bitbucket.org/nuttx/nuttx). TizenRT didn't bring all of the device drivers or any of the [NX](http://www.nuttx.org/doku.php?id=documentation:nxgraphics) graphics subsystem from NuttX. This project adds RA8875 support back into TizenRT over an SPI bus, and adds back the graphics subsystem. 

(Maybe this is a bad idea, but early indications are that it might kinda work!)

## Display

I picked up an EastRising [ER-TFTM043-3](https://www.buydisplay.com/default/4-3-tft-lcd-display-module-controller-board-serial-spi-i2c-mcu) from BuyDisplay. It has a 4.3" display with options for things like bus interfaces and touch screens. Right now this project is targeted to the SPI 4-wire, 3.3V option. Touch is a future goal.

### Wiring

All wiring is between the Artik 053 dev kit CON703 and ER-TFTM043-3 display JP1/CON1 headers. I'm using male-female ribbon jumper cables [from Amazon](https://www.amazon.com/gp/product/B01LZF1ZSZ/) but of course use whatever you want.

| Signal | Artik name | Artik pin | Display pin | Display name |
| ------ | ---------- | --------- | ----------- | ------------ |
| 3.3V   | VCC_EXT3P3 | 2         | 3           | VDD
| 3.3V   | VCC_EXT3P3 | 14        | 4           | VDD
| GND    | GND        | 12        | 1           | GND
| GND    | GND        | 24        | 2           | GND
| MOSI   | XSPI0_CLK  | 16        | 8           | SCLK
| MOSI   | XSPI0_CS   | 18        | 5           | /SCS
| MISO   | XSPI0_MISO | 20        | 6           | SDO
| MOSI   | XSPI0_MOSI | 22        | 7           | SDI

### Driver

The ra8875_spi bridge between the RA8875 LCD and SPI drivers took a bit of experimentation to get right. Here are the main things that I needed to do that seem to contradict other sources (including the RA8875 datasheet):

* Configured SPI to Mode 0. Maybe I have a bizarro RA8875 chip, or they changed the behavior without changing the docs, or my bus timing is totally wack. Everything I've read says it's Mode 3.
* Disabled software reset of the at driver initialization time. I think I was able to recover normal functionality when poking registers manually, but when run by the driver routines the SPI interface would freeze every time.
* SYSR color depth setting (bits 3,2) needs to be set to b10 instead of b11. Setting to b11 seems to transfer only the low-order byte as an 8-bit color to the display.

There's a lot of insight into the chip at https://github.com/sumotoy/RA8875/wiki, with the above caveats. In particular, it validated my thought that the R1/R2/R3 pullups and C1/C2 caps are pretty useless and are possibly detrimental to high-speed SPI operation.

## Building

The `artik053/ra8875` configuration set is preconfigured for the ER-TFTM043-3 over SPI. I haven't written up a proper example program to run through the display routines, so there's not much to see at the moment.

* Change to <root>/os/tools
* Configure the build environment with `./configure.sh artik053/ra8875`
* Change to <root>/os
* Run `make menuconfig` and change options as required. The RA8875 bits are in Device Drivers -> LCD Driver Support.
* Build with `make`

Lots more hints:
* https://developer.artik.io/documentation/artik-05x/getting-started/ and 
* https://github.com/kimvsparrow/ARTIK-05x-RA8875/blob/artik/build/configs/artik05x/README.md

If your goal is to build an SDK to export into the Artik IDE, that's a whole rigamarole that I'm probably not going to get into here.
