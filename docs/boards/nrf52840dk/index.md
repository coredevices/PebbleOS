# Nordic nRF52840 DK (nrf52840dk)

## Overview

:::{div} board-card

```{thumbnail} images/nrf52840dk.webp
```

:Board: `nrf52840dk`
:SoC: Nordic nRF52840
:::

The nRF52840 DK is the Nordic development kit for the nRF52840.

## Hardware

- **SoC**: [Nordic nRF52840](https://www.nordicsemi.com/Products/nRF52840),
  with an Arm Cortex-M4 with FPU, an integrated Bluetooth Low Energy radio,
  1 MiB flash and 256 KiB RAM
- **Flash**: 8 MiB Macronix MX25R6435F quad SPI NOR
- **Clocks**: 32 MHz and 32.768 kHz crystals
- **Buttons**: four user buttons, Button 1 to 4 as keys 1 to 4, Button 1 also
  waking the board from power-off, and a reset button
- **LEDs**: four green user LEDs, LED1 the status LED
- **USB**: full-speed USB device port, nRF USB, on micro-B
- **Debug**: SEGGER J-Link OB interface MCU, with a virtual COM port on
  UARTE0, P0.06 (TX) and P0.08 (RX), which also powers the kit
- **Connectors**: Arduino Uno R3, with its I2C bus on TWIM0, its SPI bus on
  SPIM3, its UART on UARTE1, and A0 to A5 on SAADC inputs AIN1, AIN2 and AIN4
  to AIN7; NFC antenna and current measurement headers

## Flashing

The board embeds a J-Link, through which the `nrfutil` runner flashes it.
