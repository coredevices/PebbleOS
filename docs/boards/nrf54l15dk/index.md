# Nordic nRF54L15 DK (nrf54l15dk)

## Overview

:::{div} board-card

```{thumbnail} images/nrf54l15dk.webp
```

:Board: `nrf54l15dk`
:SoC: Nordic nRF54L15
:::

The nRF54L15 DK is the Nordic development kit for the nRF54L15. PebbleOS runs
on its Arm application core, as secure code.

## Hardware

- **SoC**: [Nordic nRF54L15](https://www.nordicsemi.com/Products/nRF54L15),
  with an Arm Cortex-M33 with FPU at 128 MHz, a RISC-V coprocessor, an
  integrated Bluetooth Low Energy radio, 1524 KiB RRAM and 256 KiB RAM
- **Clocks**: 32 MHz and 32.768 kHz crystals, on the internal load
  capacitors
- **Buttons**: four user buttons, Button 0 to 3 as keys 0 to 3, and a
  reset button
- **LEDs**: four green user LEDs, LED0 the status LED
- **Debug**: SEGGER J-Link OB interface MCU, with a virtual COM port on
  UARTE20, P1.04 (TX) and P1.05 (RX), which also powers the kit

The radio, the coprocessor and the external flash are not supported yet, nor
is the random number generator of the CRACEN.

## Flashing

The board embeds a J-Link, through which the `nrfutil` runner flashes it.
