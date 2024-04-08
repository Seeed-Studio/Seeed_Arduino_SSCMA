# Grove Vision AI (WE2) Bootloader Recovery Tool Manual


## Introduction

This document describes how to recover the bootloader of the Grove Vision AI (WE2) module. The recovery tool is a software tool that can be used to recover the bootloader of the Grove Vision AI (WE2) module in case the bootloader is corrupted, or flash a new firmware to the Grove Vision AI (WE2) module.


## Prerequisites

- Any board which has I2C interface and supported by Arduino IDE.
- Grove Vision AI (WE2) module
- 4-Pin Cable


## Software Installation

1. Install the Arduino IDE from the [official website](https://www.arduino.cc/en/software).
1. Download the latest version of the Seeed_Arduino_SSCMA library from the [GitHub repository](https://github.com/your_repository_link).
1. Add the library to your Arduino IDE by selecting **Sketch > Include Library > Add .ZIP Library** and choosing the downloaded file.
1. Open the `we2_iic_bootloader_recover` example from the Arduino IDE: **File > Examples > Seeed_Arduino_SSCMA > we2_iic_bootloader_recover**.
1. Upload the example to your Arduino board.


## Hardware Connection

Connect the Grove Vision AI (WE2) module to the default I2C interface of your Arduino board using the 4-Pin Cable. Make sure each wire is connected to the correct pin.

- SCL -> SCL (Grove Vision AI WE2)
- SDA -> SDA (Grove Vision AI WE2)
- VCC -> VCC (Grove Vision AI WE2, 3.3V)
- GND -> GND (Grove Vision AI WE2)


## Usage

1. Make sure your Arduino board is connected to your computer, and the Grove Vision AI (WE2) module is connected to the Arduino board.
1. Open the Serial Monitor in the Arduino IDE. (or any other serial monitor software, e.g., minicom, PuTTY)
1. Wait for the Grove Vision AI (WE2) device to be detected
1. Press 'enter' to start the bootloader recovery process, wait for the process to complete

Note: You may need to press the 'reset' button on the Arduino board to restart the bootloader recovery process. If the bootloader recovery process fails, please check the hardware connection and reset the power supply of the Grove Vision AI (WE2) module and try again. In some cases, you may have to try 3-10 times to recover the bootloader successfully.


### FAQ

1. **Q:** What should I do if the Grove Vision AI (WE2) device is not detected?

    **A:** Make sure the Grove Vision AI (WE2) module is connected to the correct I2C interface of the Arduino board, and the power supply is stable.

2. **Q:** How long does the bootloader recovery process take?

    **A:** The bootloader recovery process takes about 10 seconds to complete.

3. **Q:** Can I use this tool to flash a new firmware to the Grove Vision AI (WE2) module?

    **A:** Yes, you can use this tool to flash a new firmware to the Grove Vision AI (WE2) module. You can use `firmware_to_c_header.ipynb` to convert the firmware binary to a C header file, and then use the `we2_iic_bootloader_recover` example to flash the new firmware to the Grove Vision AI (WE2) module.

4. **Q:** The Arduino board is not responding after the bootloader recovery process, what should I do?

    **A:** Try to press the 'reset' button on the Arduino board or disconnect the Grove Vision AI (WE2) module and reconnect it to the Arduino board.
