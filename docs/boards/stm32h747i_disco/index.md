# STMicroelectronics STM32H747I-DISCO (stm32h747i_disco)

## Overview

:::{div} board-card

:Board: `stm32h747i_disco/m7`, `stm32h747i_disco/m4`
:SoC: ST STM32H747
:Platform: none, samples only
:::

The STM32H747I-DISCO is the ST Discovery kit for the dual-core STM32H747.
PebbleOS is built for each core on its own: `stm32h747i_disco/m7` runs on the
Cortex-M7, with all of the kit described here, and `stm32h747i_disco/m4` on
the Cortex-M4, with LD2, a console on the Arduino connector and its own system
timer.

## Hardware

- **SoC**: ST STM32H747XIH6, with an Arm Cortex-M7 with double-precision FPU
  and 16 KiB L1 caches, and an Arm Cortex-M4 with a single-precision FPU
- **Memory**: 2 MiB embedded flash in two banks of 1 MiB, the Cortex-M7
  booting from bank 1 and the Cortex-M4 from bank 2, 512 KiB AXI SRAM, which
  the Cortex-M7 runs from, and 288 KiB SRAM1 to SRAM3, which the Cortex-M4
  runs from
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
  The digital microphone is on the PDM interface of SAI4 block A (clock PE2,
  data PC1), clocked at 64 times the rate from PLL2, 128 times at 8 kHz, and
  decimated in software; the BDMA moves its raw samples, through the first
  4 KiB of the SRAM4, the only RAM it reaches; it is the `mic`
- **USB**: the USB OTG HS, a DesignWare USB 2.0 OTG, on its micro-AB
  connector through the Microchip USB3320 ULPI PHY, as a high-speed device
  with `CONFIG_USB_DEVICE`; VDD33USB comes from the USB voltage detector of
  the PWR
- **Graphics**: the DMA2D (Chrom-ART) fills and blits, with format
  conversion and blending; it is the `gpu`
- **Supply**: the SoC SMPS supplies the core domain directly, as the kit
  ships (`st,supply-config = "direct-smps"`)
- **Clocks**: 25 MHz HSE and 32.768 kHz LSE crystals; PLL1 runs the
  Cortex-M7 at 400 MHz from HSE, with the AHB at 200 MHz and the APB buses at
  100 MHz; LPTIM1, the system timer, counts the LSE
- **RTC**: the calendar RTC counts the LSE in the backup domain, keeping the
  time across resets, and its alarm wakes the kit from power-off; it is the
  `rtc`
- **Debug**: ST-LINK/V3E on CN2, which also powers the kit, with its virtual
  COM port on USART1, PA9 (TX) and PA10 (RX), received through DMA
- **Other**: the RNG, from HSI48, the CRC unit, used with `CONFIG_CRC_HW`,
  the die temperature, from the sensor on ADC3 with its factory
  calibration, and the independent watchdog, from LSI, which firmware with
  `CONFIG_WATCHDOG` starts
- **Low power**: with `CONFIG_CPU_IDLE`, the Cortex-M7 idles in Stop mode,
  its D1 domain stopped and the SDRAM in self-refresh, unless the console
  receives, the display is on or the DMA2D draws; the whole system only
  stops, the HSE and the PLLs with it, when the Cortex-M4 idles in Stop too,
  see [](#stm32h747i-disco-cores)

## Buttons and LEDs

| Device | Pins | PebbleOS |
| --- | --- | --- |
| Joystick left, up, center, down | PK4, PK6, PK2, PK3, active low | Back, Up, Select and Down buttons (`gpio-keys`) |
| Joystick right | PK5, active low | Right key (`gpio-keys`), which watch software leaves alone |
| Wakeup button | PC13, active high, wakeup pin | Wakeup key (`gpio-keys`), which starts the kit again from power-off |
| LD1 to LD4, green, orange, red, blue | PI12 to PI15, active low | LEDs (`gpio-leds`), LD1 being the status LED |

(stm32h747i-disco-flashing)=

## Flashing

The board is flashed with the `openocd` runner, through the ST-LINK. Give the
serial number of the probe when other debug probes are connected, with
`--probe 0483:374e:<serial>`, as OpenOCD may pick another one otherwise. The
OpenOCD configuration connects under reset, so the probe reaches the core
whatever the firmware leaves it doing. It also keeps OpenOCD from setting up
the quad-SPI flash, which would leave the flash in a mode PebbleOS does not
expect.

The console of the Cortex-M7 runs at 1 Mbaud, on the ST-LINK virtual COM port.

(stm32h747i-disco-cores)=

## The two cores

Each core is a build of its own, flashed on its own: the Cortex-M7 from bank 1
of the embedded flash, the Cortex-M4 from bank 2.

```shell
pbl configure --board stm32h747i_disco/m7 -b build/m7 samples/drivers/display
pbl configure --board stm32h747i_disco/m4 -b build/m4 samples/drivers/led
pbl -b build/m7 run
pbl -b build/m4 run
```

The Cortex-M7 sees the Cortex-M4 as a remote processor, the `m4` node: once
the clocks are set up, it starts the Cortex-M4 if its boot address, from the
option bytes (bank 2 as the kit ships), holds an image. The Cortex-M4 waits
for it at boot, whether its boot option bit, BCM4, starts it at reset or not,
so the option bytes can stay as they ship. The Cortex-M4 runs from SRAM1 to SRAM3, 288 KiB, counts time on
LPTIM2 and has LD2, the orange LED. Its console is UART8, at 1 Mbaud, on D1
(TX) and D0 (RX) of the Arduino connector, for a USB serial adapter.

A Cortex-M4 that runs keeps the whole system out of Stop mode, and an erased
bank 2 with BCM4 set leaves it locked up. With nothing else to do, flash it an
application that idles, as `samples/kernel/idle`: its idle thread keeps it in
Stop.

The cores ring each other through the hardware semaphores, a mailbox whose
channels are the semaphores. Power-off is the Cortex-M7's, and takes the
Cortex-M4 with it: the Cortex-M7 rings the `shutdown` channel of the `m4` node,
on which the Cortex-M4 runs its own power-off, and waits for it to take the
doorbell. The Cortex-M4 holds the flash, so the domain of the Cortex-M7 stays
up until the Cortex-M4 stops too, and the whole system then reaches Standby,
every domain with it.

Debuggers turn on low-power debug in DBGMCU_CR when they attach, which keeps
the domains powered in Standby until the next power-on reset: power the kit
off and on after flashing to measure power-off.
