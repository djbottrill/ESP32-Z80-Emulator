# ESP32-Z80-Emulator

*** Update 18th April 2022 ***

V2. Improvements to CPU Task and single Step / Breakpoints.

Note
There is a problem with the V2.0.x ESP32 core that in most cases causes the SD card to perform slowly, if at all. It would appear these issues will be fixed in version 2.03 of the core which is due for imminent release.


****************************

*** Update 13th July 2021 ***

Improved WiFi connection reliability. 
****************************

*** Update 12th June 2021 ***

Various code improvements and added experimental Telnet support so you can telnet to the Z80 over WiFi. On a Telnet connection being made the Z80 will reboot, when telnet is in use output will also be mirrored to the serial console.
****************************

*** Update 7th June 2021 ***

Fixed nasty bug that affected multiplication in MBASIC and the ability to enter commands in Zork. Add, subtract, compare and DAA instructions have been improved.
Now supports the TTGO-T2 board with colour OLED the display currently implements RED and Green 'virtual' LEDs to show the bit status of both GPIO ports and a blue 'virtual' LED to indicate disc activity.
****************************


This is very much a work in progress however Nascom Basic and CP/M 2.2 do run quite well.
It's not pretty but it does work !!

On a techincal note the serial receive code has been put in a separate task on the ESP32 the reason is that just checking for a received character really slows down the emulator so this has been relegated to a task running on Core 0 of the ESP so it doesn't impact the main emulator code running on Core 1.

The ESP32 code is configured for a Lilygo TTGO-T1 board with integrated SD card reader although this should run on most ESP32 Dev boards however it may be necessary to re-confiure the SPI ports for the SD card and also the GPIO Pins assigned to Z80 GPIO Ports and breakpoint switches.

The emulator handles all the original 8080 derived Z80 instructions and also implements some of the additional Z80 instructions, all in all over 350 instructions, enough to get Nascom basic and CP/M running although BBC Basic doesn't currently run under CP/M however mbasic does. For those interrested in retro games Zork also runs fine.

I haven't had time to perform benchmarks but comparing it to a homemade Z80 SBC running at 12Mhz it seems very comparable in it's performance.

The system boots from either SD Card or SPIFFS and initially looks for a file boot.txt on the SD card this is a simple text file that lists the rom images and the address in Hex to where they should be loaded. if no SD card is found then boot.txt is read from SPIFFS. Although it would be possible to put the CP/M ROM images on SPIFFS there would be insufficient space for a disk image and this would not be recommended for wear reasons on SPIFFS, and anyway the virtual disk controller is only configured for SD cards.

This version of CP/M can address 16 disks A - P of 8.4MB, each disk is held as a single file on the SD card in the /disks folder i.e. A.dsk, B.dsk...... P.dsk.
I've only included one disk image bit this can be cloned; format.com will erase any disk.


Additional CP/M utilities are provided:

sdfies.com  - Lists files in the current SD card directory, defaults to /downloads

sdpath.com - sets the SD card path, defaults to /downloads.

sdcopy.com - copies a file on the SD card to CP/M disk


Switch one should be a normally off toggle switch and when closed enables single step mode, switch 2 is a push button and is used to step to the next instruction or breakpoint.

The variable BP sets the breakpoint address.

The variable BPMode sets the Breakpoint action:
Mode 0 - Halt immediately and single step thereafter until switch one is turned off
Mode 1 - Halt at breakpoint and single step thereafter
Mode 2 - Stop each time the breakpoint is reached

Whenver the Z80 stops an a breakpoint all registers are dumped to the console along with the next 4 bytes of code and the top stack entry.

If switch one is on when the ESP32 starts then breakpoint mode is enabled, note that this slows the emulator down by around 50%. Pressing switch 2 will start the Z80 allowing you to turn switch one off if you don't want the CPU to stop at the first breakpoint straight away.
From V1.2 I've added the ability to set the Breakpoint address and mode interractively on startup through the console. Just boot the ESP32 with switch one on.

The emulator has a virtual 6850 UART that has a base address of 0x80 the baud rate is fixed by the emulator sofware and is configure for 115200 baud.
The Arduino serial monitor is unsuitable from interractive sessions and I would recommend "Serial" on OSX, "Putty" on windows or "Miniterm" on Linux. 

The emulator has 2 virtual Z80 GPIO Ports, Port A is at 0x00 and is 8 bits in size and ports B is at 0x82 with 2 bits. This should be enough to allow parallel data transfer with the Port B pins being used to strobe and acknowledge.
Ports 0x01 and 0x03 allow the individual IO port pins to be configured as inputs or outputs setting the bits in this port to 1 configure the the matching IO port bit to output and 0 to input. All ports are by default outputs.

The Nascom Basic and CP/M 2.2 code is heavilly based on the work of Grant Searle in particular his 7 chip Z80 SBC and RC-14 computer. the Z80 UART Monitor can be found here:: https://github.com/fiskabollen/z80Monitor


Note for Lilygo TTGO-T1 users the maximum sketch upload speed in the Arduino IDE seems to be 230400 baud and code uploads sometimes fails if there is an SD card in the reader so you may need to eject the card before uploading the sketch.


