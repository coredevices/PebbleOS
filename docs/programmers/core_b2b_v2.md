# Core B2B v2

The Core B2B v2 is the programmer of the Pebble 2 Duo firmware development
kit. It plugs into the watch's B2B (board-to-board) connector and gives a
host SWD debug access and the debug UART over a single USB connection.

:Boards: {doc}`asterix <../boards/asterix/index>`
:Host interface: USB: CMSIS-DAP probe and virtual COM port
:Target interfaces: SWD, debug UART, reset, VUSB
:Runners: `openocd`, `nrfutil`, `probe-rs`
:Firmware: [coredevices/asterix_b2b2_fw](https://github.com/coredevices/asterix_b2b2_fw)

It is based on the [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html).

## Features

```{figure} images/core-b2b-v2-board.webp
Core B2B v2
```

The flex cable on the left goes to the watch's B2B connector.

2. USB connector

   - Powers the board
   - Provides VUSB
   - Exposes a CMSIS-DAP device and a virtual COM port

3. Debug UART routing

   - L: External connector (5)
   - R: Embedded virtual COM port (2)

4. SWD routing

   - L: External SWD connector (6)
   - R: Embedded CMSIS-DAP (2)

5. Debug UART pins
6. External SWD connector
7. MCU reset
8. VUSB switch

   - L: connected
   - R: disconnected

```{warning}
External connectors (SWD/UART) expose 1.8V signals.
Do not use any adapter that does not operate at 1.8V, or there is a **risk of damage**!
```

## Usage

Before first use, update the programmer to the
[latest firmware release](https://github.com/coredevices/asterix_b2b2_fw/releases).

To use the built-in CMSIS-DAP probe and USB-UART converter, place switches
(3) and (4) to the right. Provide VUSB while programming, so the watch stays
programmable regardless of its battery level, by placing switch (8) to the
left:

```{figure} images/core-b2b-v2-switches.webp
Switches set for the built-in probe and USB-UART converter, with VUSB
provided
```

The board page explains how to connect it: {doc}`../boards/asterix/index`.

## Troubleshooting

- If the watch cannot be programmed, check the switches; they sometimes
  move during shipping.
- If the serial console shows output but does not accept input, the B2B
  connector is not fully seated. Press it until it clicks.
