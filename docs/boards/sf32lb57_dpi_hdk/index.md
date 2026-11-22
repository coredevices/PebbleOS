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
- **Microphones**: two analog MEMS microphones, U9 on ADC1P with H46 and U10
  on PA10 (ADC2P) with H47, powered from the codec's microphone bias
- **Speaker**: Nsiway NS4150B class D amplifier on the codec DAC, to J12, with
  H42 for its 5 V supply and H43 on 1-2 for its enable on PA51
- **USB**: WCH CH343P USB to UART bridge on the debug UART, and a full-speed
  USB port, both on USB-C
- **Storage**: microSD card slot
- **Debug**: SWD header
- **CAN**: CAN1 on header H59 (RX on PA09, TX on PA08), with 3.3 V on H60,
  for an external transceiver
- **Connectors**: 8080, quad SPI and RGB LCD, DVP camera, PDM microphones,
  and pin headers with the GPIOs
- **Power**: from USB or a battery. Both USB-C connectors, through H49 and
  H56, feed the VBUS input of the PMU, whose charger tells the USB controller
  whether USB power is present: with the DBG connector plugged in, it always is

## Add-ons

### SiFli 1.85" AMOLED module

[`sifli_lcm_1p85`](../../addons/sifli_lcm_1p85/index.md), on the quad SPI LCD
connector, with jumpers H14, H15, H16 and H20 fitted.

## Flashing

The board embeds a USB to UART converter, on the DBG USB-C connector,
connected to the debug UART, through which the `sftool` runner flashes it.
