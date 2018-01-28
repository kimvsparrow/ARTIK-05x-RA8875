# TizenRT for ARTIK05x w/ RA8875

This project forks [TizenRT](https://github.com/SamsungARTIK/TizenRT) from [SamsungARTIK](https://github.com/SamsungARTIK), the ARTIK 05x-focused fork of [TizenRT 1.0](https://github.com/Samsung/TizenRT) from [SamsungOpenSource](https://github.com/Samsung).

TizenRT is a lightweight RTOS-based platform to support low-end IoT devices. It is largely based on [NuttX](https://bitbucket.org/nuttx/nuttx). TizenRT didn't bring all of the device drivers or any of the [NX](http://www.nuttx.org/doku.php?id=documentation:nxgraphics) graphics subsystem from NuttX. This project adds RA8875 support back into TizenRT over an SPI bus, and adds back the graphics subsystem. 

Maybe this is a bad idea, but early indications are that it might kinda work!
