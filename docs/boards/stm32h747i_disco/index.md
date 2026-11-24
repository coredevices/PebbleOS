# STMicroelectronics STM32H747I-DISCO (stm32h747i_disco)

## Overview

:::{div} board-card

```{thumbnail} images/stm32h747i_disco.webp
```

:Board: `stm32h747i_disco/m7`, `stm32h747i_disco/m4`
:SoC: ST STM32H747
:::

The STM32H747I-DISCO is the ST Discovery kit for the dual-core STM32H747.
Each core is a board of its own: `stm32h747i_disco/m7` for the Cortex-M7 and
`stm32h747i_disco/m4` for the Cortex-M4.

## Hardware

- **SoC**: ST STM32H747XIH6, with an Arm Cortex-M7 with double-precision FPU
  and an Arm Cortex-M4 with single-precision FPU, 2 MiB flash and 1 MiB RAM
- **SDRAM**: 32 MiB
- **Flash**: two 64 MiB Quad-SPI NOR flashes
- **Clocks**: 25 MHz and 32.768 kHz crystals
- **Display**: 4" 800 × 480 TFT LCD with capacitive touch, on a MIPI DSI
  daughterboard
- **Audio**: SAI audio codec with a headset jack, and digital MEMS
  microphones
- **Ethernet**: 10/100 Mbit/s, on RJ45
- **USB**: USB OTG high-speed, on micro-AB
- **Storage**: microSD card slot
- **Buttons**: four-way joystick with select, a user button and a reset
  button
- **LEDs**: four user LEDs, green, orange, red and blue
- **Debug**: ST-LINK/V3E, with a virtual COM port, which also powers the kit
- **Connectors**: Arduino Uno V3, STMod+ and 8-bit camera

## Flashing

The board embeds an ST-LINK, through which the `openocd` runner flashes it.
