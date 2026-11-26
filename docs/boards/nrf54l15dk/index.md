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
- **Power**: VREGMAIN runs from its DC/DC converter, whose inductor is fitted
- **Buttons**: four user buttons, Button 0 to 3 as keys 0 to 3, and a
  reset button
- **LEDs**: four green user LEDs, LED0 the status LED
- **Flash**: 8 MiB Macronix MX25R6435F SPI NOR, on SPIM00 (P2.01 SCK,
  P2.02 MOSI, P2.04 MISO, P2.05 CS) at 8 MHz
- **Crypto**: the CRACEN of the SoC, for random numbers, SHA-256 and AES
- **Debug**: SEGGER J-Link OB interface MCU, with a virtual COM port on
  UARTE20, P1.04 (TX) and P1.05 (RX), which also powers the kit

The radio runs the Nordic SoftDevice Controller from the nonfree libraries,
under the NimBLE host. The coprocessor is not supported yet.

## Flashing

The board embeds a J-Link, through which the `nrfutil` runner flashes it.
