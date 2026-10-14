# Pebble Round 2 (getafix)

## Overview

:::{div} board-card

```{image} images/getafix.webp
:alt: Pebble Round 2
```

:Board: `getafix`
:Revisions: `dvt`, `dvt2`
:SoC: SiFli SF32LB52
:Platform: Gabbro
:Emulator: `qemu_gabbro`
:::

Pebble Round 2 is a round color watch with a touchscreen, built around the
SiFli SF32LB52. Its firmware targets the Gabbro platform, which apps see as
a 260 × 260 round color display.

## Hardware

- **SoC**: [SiFli SF32LB52](https://www.sifli.com/en/sf32lb52x), with an Arm STAR-MC1 (Armv8-M) application
  core with FPU, a Bluetooth Low Energy controller on its own core, and
  512 KiB SRAM
- **Flash**: [GigaDevice GD25Q256E](https://www.gigadevice.com/product/flash/spi-nor-flash/gd25q256e), 32 MiB NOR
- **Display**: 260 × 260, 64-color round JDI memory-in-pixel, driven by
  the SF32LB52 LCD controller
- **Backlight**: [Awinic AW9364E](https://www.awinic.com/en/productDetail/AW9364EDNR) LED driver
- **Touch**: Hynitron CST816 capacitive touch controller
- **Accelerometer**: [ST LIS2DW12](https://www.st.com/en/mems-and-sensors/lis2dw12.html)
- **Magnetometer**: [MEMSIC MMC5603NJ](https://www.memsic.com/magnetometer-2)
- **Ambient light**: W1160
- **Vibration**: [Awinic AW86225](https://www.awinic.com/en/productDetail/AW86225CSR) haptic driver (`dvt2`), or Awinic AW8623X
  (`dvt`), with a linear resonant actuator
- **Microphone**: PDM, on the SF32LB52 PDM interface
- **Power**: [Nordic nPM1300](https://www.nordicsemi.com/Products/nPM1300) PMIC and battery charger

### Revisions

| Revision | Description |
| --- | --- |
| `dvt` | Design validation units, with an AW8623X haptic driver |
| `dvt2` | Second design validation build, with an AW86225 haptic driver |

## Connecting a programmer

Pebble Round 2 is programmed with a {doc}`../../programmers/pblprog_sifli`, over
the watch's debug UART:

1. Connect the programmer's flex cable to the B2B connector on the watch
   mainboard.
2. Connect the programmer to the host over USB-C.
3. Turn the `CHG` switch on to power the watch from USB while programming,
   so it stays reachable whatever its battery level.

The programmer's page describes its connector, reset and charge switch.

Once connected, the {doc}`pbl commands <../../pbl/index>` flash the watch
and reach its console.
