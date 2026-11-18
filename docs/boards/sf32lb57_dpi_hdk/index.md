# SiFli SF32LB57x-DPI-HDK (sf32lb57_dpi_hdk)

## Overview

:::{div} board-card

```{image} images/sf32lb57_dpi_hdk.webp
:alt: SiFli SF32LB57x-DPI-HDK
```

:Board: `sf32lb57_dpi_hdk`
:SoC: SiFli SF32LB57
:Platform: none, samples only
:::

The SF32LB57x-DPI-HDK is the SiFli development kit for the SF32LB57, with
the SoC on a plug-in module. PebbleOS supports it with the SF32LB57-V-MOD-M2-E
module, which carries an SF32LB579V6EN6, and runs the samples on it: the
console, the kernel and its system timer work so far.

## Hardware

- **SoC**: SiFli SF32LB579V6EN6, with two Arm STAR-MC1 (Armv8-M) application
  cores with FPU in the high-performance subsystem, of which PebbleOS runs on
  the first, a low-power subsystem with its own core and the Bluetooth
  controller, and 512 KiB SRAM
- **Flash**: 16 MiB Puya PY25Q128HA NOR in the SoC package, on MPI2, which the
  boot ROM boots from, and another on the module, on MPI3
- **PSRAM**: 16 MiB HyperRAM in the SoC package, on MPI1, supplied by LDO18
- **Clocks**: 48 MHz crystal; the 32.768 kHz crystal is not fitted
- **Debug UART**: PA18 (RX) and PA19 (TX), to a WCH CH343P USB to UART bridge

## Connectors

| Connector | Type | Signals |
| --- | --- | --- |
| FPC2 (LCDC-QSPI) | SiFli quad SPI LCD | LCDC1 through the FreeIO matrix (PA42-PA50), I2C1 on PA22/PA23 with H15/H16, reset PA43, TE PA47, touch interrupt PA06 and reset PA07; 3.3 V with H20 |

FPC1 (8080) and FPC3 (40-pin RGB) share pads with FPC2, so one of them works
at a time; only FPC2 is described.

## Add-ons

### SiFli 1.85" AMOLED module

[`sifli_lcm_1p85`](../addons/sifli_lcm_1p85/index.md), on FPC2, with these
jumpers fitted:

- H20, for the 3.3 V supply of the connector.
- H15 and H16, for the touchscreen I2C on PA22 and PA23.
- H14, for its pull-ups.

The panel is upright with its flex on the right. A frame of 390x450 in
RGB565 is reserved for it at the start of the PSRAM, so an application
using the display enables the PSRAM too, with `CONFIG_RAM=y`. The display
takes ARGB2222 framebuffers unless the application picks
`CONFIG_DISPLAY_SF32LB57_LCDC_FORMAT_RGB565=y`.

## Flashing

The board is flashed with the `sftool` runner, over the debug UART, which
needs sftool 0.2.5 or later. sftool attaches through the SoC debugger, which
listens to the receiver of USART1 on PA18 and PA19, which the USART driver
keeps on. If sftool does not connect, as when the firmware is in deep sleep
or hung, turn the `BOOT` switch on and press `RESET`, so that the boot ROM
stays in download mode.

The firmware boots from the flash table at the start of the SoC flash,
which replaces the one of the factory firmware.
