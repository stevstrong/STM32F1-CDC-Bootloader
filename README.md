# STM32F1-CDC-Bootloader
### A bootloader based on USB CDC protocol for STM32F1 family MCUs.



The repository contains an Eclipse project in which the project can be built.

Features:
- no special drive installation: the device with this bootloader will enumerate as a serial COM port.
- the source files are partially based on libmaple core files, also included in this repository.
- the binary size of the bootloader is 4092 bytes so that is fits in the lower 4 memory pages of EEPROM.
- in order to upload a program with the bootloader, a special utility program is needed, see CDC flasher.
