# Nordic nRF52 DK (nrf52dk)

## Overview

:::{div} board-card

```{thumbnail} images/nrf52dk.webp
```

:Board: `nrf52dk`
:SoC: Nordic nRF52832
:::

The nRF52 DK is the Nordic development kit for the nRF52832.

## Hardware

- **SoC**: [Nordic nRF52832](https://www.nordicsemi.com/Products/nRF52832),
  with an Arm Cortex-M4 with FPU, an integrated Bluetooth Low Energy radio,
  512 KiB flash and 64 KiB RAM
- **Clocks**: 32 MHz and 32.768 kHz crystals
- **Buttons**: four user buttons, Button 1 to 4 as Back, Up, Select and Down,
  and a reset button
- **LEDs**: four green user LEDs, LED1 the status LED
- **Debug**: SEGGER J-Link OB interface MCU, with a virtual COM port on
  UARTE0, P0.06 (TX) and P0.08 (RX), which also powers the kit
- **Connectors**: Arduino Uno R3, with its I2C bus on TWIM0, its SPI bus on
  SPIM2, and A0 to A5 on SAADC inputs AIN1, AIN2 and AIN4 to AIN7; the
  buttons and LEDs share D2 to D9; NFC antenna and current measurement
  headers

The kit has no external flash, so samples that store files do not run on it.

## Flashing

The board embeds a J-Link, through which the `nrfutil` runner flashes it.
