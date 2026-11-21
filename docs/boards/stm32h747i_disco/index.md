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
random number generator, the watchdog, DMA, the display, the touchscreen and the
DMA2D work so far.

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
  two MIPI DSI lanes at 500 Mbit/s, in command mode; the LTDC sends an
  800x480 RGB565 frame to the frame memory of the panel in two halves timed
  by its tearing effect, at a 42 MHz pixel clock from PLL3, and the panel
  takes it turned a quarter turn, landscape as the kit is held; it is the
  `display`, and its controller dims the backlight of the module, the
  `backlight`
- **Touch**: the FocalTech FT6206 of the MB1166 module, on I2C4 (PD12 and
  PD13) at 400 kHz, its interrupt on PK7; it is the `touch`
- **Audio**: the Cirrus Logic WM8994 codec, on I2C4, plays to the headphone
  jack from SAI1 block A, the master of their I2S link, with its master
  clock at 256 times the rate from PLL2 at 73.75 MHz; it is the `speaker`.
  The digital microphone, on SAI4 as PDM, is not supported yet
- **Graphics**: the DMA2D (Chrom-ART) fills and blits, with format
  conversion and blending; it is the `gpu`
- **Supply**: the SoC SMPS supplies the core domain directly, as the kit
  ships (`st,supply-config = "direct-smps"`)
- **Clocks**: 25 MHz HSE and 32.768 kHz LSE crystals; PLL1 runs the
  Cortex-M7 at 400 MHz from HSE, with the AHB at 200 MHz and the APB buses at
  100 MHz; LPTIM1, the system timer, counts the LSE
- **RTC**: the calendar RTC counts the LSE in the backup domain, keeping the
  time across resets; it is the `rtc`
- **Debug**: ST-LINK/V3E on CN2, which also powers the kit, with its virtual
  COM port on USART1, PA9 (TX) and PA10 (RX), received through DMA
- **Other**: the RNG, from HSI48, and the independent watchdog, from LSI,
  which firmware with `CONFIG_WATCHDOG` starts
- **Low power**: with `CONFIG_CPU_IDLE`, the Cortex-M7 idles in Stop mode,
  its D1 domain stopped and the SDRAM in self-refresh, unless the console
  receives, the display is on or the DMA2D draws; the whole system only
  stops, the HSE and the PLLs with it, when the Cortex-M4 is gated too, see
  [](#stm32h747i-disco-flashing)

## Buttons and LEDs

| Device | Pins | PebbleOS |
| --- | --- | --- |
| Joystick left, up, center, down | PK4, PK6, PK2, PK3, active low | Back, Up, Select and Down buttons (`gpio-keys`) |
| Wakeup button | PC13, active high, wakeup pin | Wakeup key (`gpio-keys`), which starts the kit again from power-off |
| LD1 to LD4, green, orange, red, blue | PI12 to PI15, active low | LEDs (`gpio-leds`), LD1 being the status LED |

Joystick right (PK5) is not described: a watch has four buttons.

(stm32h747i-disco-flashing)=

## Flashing

The board is flashed with the `openocd` runner, through the ST-LINK. Give the
serial number of the probe when other debug probes are connected, with
`--probe 0483:374e:<serial>`, as OpenOCD may pick another one otherwise. The
OpenOCD configuration connects under reset, so the probe reaches the core
whatever the firmware leaves it doing. It also keeps OpenOCD from setting up
the quad-SPI flash, which would leave the flash in a mode PebbleOS does not
expect.

PebbleOS does not use the Cortex-M4, which keeps the whole system out of Stop
mode while it runs. Clear its boot option bit, BCM4, once:

```shell
openocd -s boards/stm32h747i_disco/support -f openocd.cfg \
  -c "init; reset halt; stm32h7x option_write 0 0x20 0 0x00400000; shutdown"
```

The console runs at 1 Mbaud, on the ST-LINK virtual COM port.
