# ESP32-Z80-Emulator

This is very much a work in progress however Nascom Basic and CP/M 2.2 do run quite well.
It's not pretty but it does work !!

The ESP32 code is configured for a Lilygo TTGO-T2 board with integrated SD card reader although this can be used on most ESP32 Dev boards however it may be necessary to re-confiure the SPI ports for the SD card and also the GPIO Pins assigned to Z80 GPIO Ports and breakpoint switches.

The emulator handles all the original 8080 derived Z80 instruction and also implement some of the additional Z80 opcodes enough to get the basis running although BBC Basic doesn't currently run, mbasic does however run.

This version od CP/M can address 16 disks A - P of 8.4MB each each disk is held as a single file on the SD card in the /disks folder i.e. A.dsk, B.dsk...... P.dsk.
I've only included one disk image bit this can be cloned; format.com will erase any disk.

Additional CP'M utilities are provided:

sdfies.com  - Lists files in the current SD card directory, defaults to /downloads
sdpath.com - sets the SD card path, defaults to /downloads
sdcopy.com - copies a file on the SD card to CP/M disk

Switch one should be a normally off toggle switch and when closed enables single step mode switch 2 is a push button and is used to step to the next instruction or breakpoint.

The variable BP sets the breakpoint address.
The variable BPMode sets the Breakpoint action:
Mode 0 - Halt immediatlye and single setp
Mode 1 - Halt and breakpoint and single setp thereafter
Mode 2 - Steop each time the breakpoint is reached

The emulator has a virtual 6850 UART that has a base address of 0x80 the baud rate is fixed by the emulator sofware as is configure for 115200 baud.

The emulator has 2 virtual Z80 GPIO Ports, Port A is at 0x00 and is 8 bits in size and ports B is at 0x82 with 2 bits. This should be enough to allow parallel data transfer with the Port B pins being used to strobe and acknowledge.
POrts 0x01 and 0x03 allow the individual IO port pins to be configure as inputs or outputs setting the bits on this port to 1 configure the the matchin IO port bit to output and 0 to input. All ports are by default outputs.

The Nascom Basic and CP/M 2.2 code is heavilly based on the work of Grant Searle in particular his 7 chip Z80 SBC and RC-14 computer.




