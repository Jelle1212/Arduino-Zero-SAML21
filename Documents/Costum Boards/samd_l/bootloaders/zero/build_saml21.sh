#!/bin/bash -ex

make clean

# With SDCARD_DISABLED
BOARD_ID=arduino_zero MCU=SAML21G18B SDCARD=SDCARD_DISABLED make all mostly_clean
BOARD_ID=arduino_zero MCU=SAMD21G18A SDCARD=SDCARD_DISABLED make all mostly_clean




# With SDCARD_ENABLED

BOARD_ID=arduino_zero MCU=SAML21G18B SDCARD=SDCARD_ENABLED make all mostly_clean
BOARD_ID=arduino_zero MCU=SAMD21G18A SDCARD=SDCARD_ENABLED make all mostly_clean



mv -v *.bin ./binaries/

echo Done building bootloaders!

