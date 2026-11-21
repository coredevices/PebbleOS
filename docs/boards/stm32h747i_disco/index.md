# STMicroelectronics STM32H747I-DISCO (stm32h747i_disco)

## Overview

:::{div} board-card

:Board: `stm32h747i_disco`
:SoC: ST STM32H747
:Platform: none, samples only
:::

The STM32H747I-DISCO is the ST Discovery kit for the dual-core STM32H747.
PebbleOS runs the samples on its Cortex-M7: the console, the kernel and its
system timer, the LEDs, the joystick, the quad-SPI NOR flash, the SDRAM, the
random number generator, the watchdog, DMA and the display work so far.

## Hardware

- **SoC**: ST STM32H747XIH6, with an Arm Cortex-M7 with double-precision FPU
  and 16 KiB L1 caches, which PebbleOS runs on, and an Arm Cortex-M4, left to
  the image in flash bank 2
- **Memory**: 2 MiB embedded flash in two banks of 1 MiB, the Cortex-M7
  booting from bank 1, and 512 KiB AXI SRAM, which PebbleOS runs from
- **External RAM**: 32 MiB ISSI IS42S32800J SDRAM, 32 bits wide, on FMC SDRAM
  bank 2 at 100 MHz, swapped to 0x70000000, where the CPU caches it; it is
  the `extram`
- **External flash**: two 64 MiB Micron MT25QL512ABB quad-SPI NOR flashes on
  the QUADSPI banks; PebbleOS uses the one on bank 1 as its `flash`, with
  quad I/O at 50 MHz
- **Display**: the MB1166 module, a 4" 480x800 Orise Tech OTM8009A panel on
  two MIPI DSI lanes at 500 Mbit/s, in video mode; the LTDC scans an RGB565
  framebuffer at the start of the SDRAM, at a 27.5 MHz pixel clock from PLL3;
  it is the `display`
- **Supply**: the SoC SMPS supplies the core domain directly, as the kit
  ships (`st,supply-config = "direct-smps"`)
- **Clocks**: 25 MHz HSE and 32.768 kHz LSE crystals; PLL1 runs the
  Cortex-M7 at 400 MHz from HSE, with the AHB at 200 MHz and the APB buses at
  100 MHz
- **Debug**: ST-LINK/V3E on CN2, which also powers the kit, with its virtual
  COM port on USART1, PA9 (TX) and PA10 (RX), received through DMA
- **Other**: the RNG, from HSI48, and the independent watchdog, from LSI,
  which firmware with `CONFIG_WATCHDOG` starts

## Buttons and LEDs

| Device | Pins | PebbleOS |
| --- | --- | --- |
| Joystick left, up, center, down | PK4, PK6, PK2, PK3, active low | Back, Up, Select and Down buttons (`gpio-keys`) |
| LD1 to LD4, green, orange, red, blue | PI12 to PI15, active low | LEDs (`gpio-leds`), LD1 being the status LED |

Joystick right (PK5) and the User button (PC13) are not described: a watch
has four buttons.

## Flashing

The board is flashed with the `openocd` runner, through the ST-LINK. Give the
serial number of the probe when other debug probes are connected, with
`--probe 0483:374e:<serial>`, as OpenOCD may pick another one otherwise. The
OpenOCD configuration connects under reset, so the probe reaches the core
whatever the firmware leaves it doing. It also keeps OpenOCD from setting up
the quad-SPI flash, which would leave the flash in a mode PebbleOS does not
expect.

The console runs at 1 Mbaud, on the ST-LINK virtual COM port.
