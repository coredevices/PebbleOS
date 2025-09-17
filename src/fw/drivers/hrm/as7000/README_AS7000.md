Some Information About the AMS AS7000
=====================================

The documentation about the AS7000 can be a bit hard to follow and a bit
incomplete at times. This document aims to fill in the gaps, using
knowledge gleaned from the datasheet, Communication Protocol document
and the SDK docs and sources.

What is the AS7000 exactly?
---------------------------

The AS7000 is a Cortex-M0 SoC with some very specialized peripherals.
It can be programmed with a firmware to have it function as a heart-rate
monitor. It doesn't necessarily come with the HRM firmware preloaded, so
it is not one of those devices that you can treat as a black-box piece
of hardware that you just power up and talk to.

There is 32 kB of flash for the main application, and some "reserved"
flash containing a loader application. There is also a ROM first-stage
bootloader.

Boot Process
------------

The chip is woken from shutdown by pulling the chip's GPIO8 pin low. The
CPU core executes the ROM bootloader after powerup or wake from
shutdown. The bootloader's logic is apparently as follows:

    if loader is available:
        jump to loader
    elif application is valid:
        remap flash to address 0
        jump to application
    else:
        wait on UART?? (not documented further)

We will probably always be using parts with the loader application
preprogrammed by AMS so the existence of the bootloader just makes the
communication protocol document a bit more confusing to understand. It
can be safely ignored and we can pretend that the loader application is
the only bootloader.

Once control is passed from the ROM bootloader to the loader, the loader
performs the following:

    wait 30 ms
    if application is valid and GPIO8 is low:
        remap flash to address 0
        jump to application
    else:
        run loader application

The reason for the 30 ms wait and check for GPIO8 is to provide an
escape hatch for getting into the loader if a misbehaving application
is programmed which doesn't allow for a command to be used to enter the
loader.

The "Application is valid" check is apparently to check that address
0x7FFC (top of main flash) contains the bytes 72 75 6C 75.

The Loader
----------

The loader allows new applications to be programmed into the main flash
over I2C by sending Intel HEX records(!). The communication protocol
document describes the protocol well enough, though it is a bit lacking
in detail as to what HEX records it accepts. Luckily the SDK comes with
a header file which fills in some of the details, and the SDK Getting
Started document has some more details.

  - The supported record types are 00 (Data), 01 (EOF), 04 (Extended
    Linear Address) and 05 (Start Linear Address).
  - The HEX records must specify addresses in the aliased range 0-0x7FFF
  - HEX records must be sent in order of strictly increasing addresses.

A comment in loader.h from the SDK states that the maximum record size
supported by the loader can be configured as 256, 128, 64 or 32. The
`#define` in the same header which sets the max record length is set
to 256, strongly implying that this is the record length limit for the
loader firmware which is already programmed into the parts. Programming
an application firmware is successful when using records of length 203,
which seems to confirm that the max record length is indeed 256.

The HEX files provided by AMS appear to be standard HEX files generated
by the Keil MDK-ARM toolchain. http://www.keil.com/support/docs/1584.htm

The Start Linear Address record contains the address of the "pre-main"
function, which is not the reset vector. Since the reset vector is
already written as data in the application's vector table, the record is
unnecessary for flash loading and is likely just thrown away by the
loader.

Empirical testing has confirmed that neither the Start Linear Address
nor Extended Linear Address records need to be sent for flashing to
succeed. It is acceptable to send only a series of Data records followed
by an EOF record.

The loader will exit if one of the following conditions is true:

  - An EOF record is sent
  - A valid HEX record is sent which the loader doesn't understand
  - Data is sent which is not a valid HEX record
  - No records are sent for approximately ten seconds

The loader exits by waiting one second for the host controller to read
out the exit code register, then performs a system reset.

The Application
---------------

Due to the simplicity of the CM0 and the limited resources available on
the chip, the application firmware takes full control of the system. The
applications in the SDK are written as a straightforward mainloop with
no OS. Think of it as a very simple form of cooperative multitasking.

The "mandatory" I2C register map mentioned in the Communication Protocol
document is merely a part of the protocol; it is entirely up to the
application firmware to properly implement it. The Application ID
register doesn't do anything special on its own: the mainloop simply
polls the register value at each iteration to see if it needs to start
or stop any "apps" (read: modules). The application firmware could
misbehave, resulting in writes to that register having no effect.

Writing 0x01 to the Application ID register isn't special either; it is
up to the application to recognize that value and reset into the loader.
That's why the escape hatch is necessary, and the fundamental reason why
the loader application cannot run at the same time as any other
application.

GPIO8 isn't special while the firmware is running, either.
