# SiFli SF32LB57x-DPI-HDK (sf32lb57_dpi_hdk)

## Overview

:::{div} board-card

```{thumbnail} images/sf32lb57_dpi_hdk.webp
```

:Board: `sf32lb57_dpi_hdk`
:SoC: SiFli SF32LB57
:::

The SF32LB57x-DPI-HDK is the SiFli development kit for the SF32LB57, with
the SoC on a plug-in module. PebbleOS supports it with the SF32LB57-V-MOD-M2-E
module, which carries an SF32LB579V6EN6.

## Hardware

- **SoC**: SiFli SF32LB579V6EN6, with two Arm STAR-MC1 (Armv8-M) application
  cores with FPU, a Bluetooth Low Energy controller on its own core, and
  512 KiB SRAM
- **Flash**: 16 MiB Puya PY25Q128HA NOR in the SoC package, and two more on
  the module
- **PSRAM**: 16 MiB HyperRAM in the SoC package
- **Clocks**: 48 MHz crystal
- **Buttons**: three user buttons and a reset button
- **Microphones**: two analog MEMS microphones
- **Speaker**: Nsiway NS4150B class D amplifier, with a speaker connector
- **USB**: WCH CH343P USB to UART bridge on the debug UART, and a full-speed
  USB port, both on USB-C
- **Storage**: microSD card slot
- **Debug**: SWD header
- **Connectors**: 8080, quad SPI and RGB LCD, DVP camera, PDM microphones,
  and pin headers with the GPIOs
- **Power**: from USB or a battery

## Add-ons

### SiFli 1.85" AMOLED module

[`sifli_lcm_1p85`](../../addons/sifli_lcm_1p85/index.md), on the quad SPI LCD
connector, with jumpers H14, H15, H16 and H20 fitted.

## Flashing

The board embeds a USB to UART converter, on the DBG USB-C connector,
connected to the debug UART, through which the `sftool` runner flashes it.
