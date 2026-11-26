# Nordic nRF54LM20 DK (nrf54lm20dk)

## Overview

:::{div} board-card

```{thumbnail} images/nrf54lm20dk.webp
```

:Board: `nrf54lm20dk`
:SoC: Nordic nRF54LM20A
:::

The nRF54LM20 DK is the Nordic development kit for the nRF54LM20A. PebbleOS
runs on its Arm application core, as secure code.

## Hardware

- **SoC**: [Nordic nRF54LM20A](https://www.nordicsemi.com/Products/nRF54LM20A),
  with an Arm Cortex-M33 with FPU at 128 MHz, a RISC-V coprocessor, an
  integrated Bluetooth Low Energy radio, a high-speed USB controller,
  2036 KiB RRAM and 512 KiB RAM
- **Clocks**: 32 MHz and 32.768 kHz crystals, on the internal load
  capacitors
- **Power**: VREGMAIN runs from its DC/DC converter, whose inductor is fitted
- **Buttons**: four user buttons, Button 0 to 3 as keys 0 to 3, Button 0 also
  waking the board from power-off, and a reset button
- **LEDs**: four green user LEDs, LED0 the status LED
- **Crypto**: the CRACEN of the SoC, for random numbers, SHA-256 and AES
- **Debug**: SEGGER J-Link OB interface MCU, with a virtual COM port on
  UARTE20, P1.16 (TX) and P1.17 (RX), which also powers the kit

The radio runs the Nordic SoftDevice Controller from the nonfree libraries,
under the NimBLE host. The external flash, the USB controller and the
coprocessor are not supported yet.

## Flashing

The board embeds a J-Link, through which the `nrfutil` runner flashes it.
