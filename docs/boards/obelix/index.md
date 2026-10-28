# Pebble Time 2 (obelix)

## Overview

:::{div} board-card

```{image} images/obelix.webp
:alt: Pebble Time 2
```

:Board: `obelix`
:Revisions: `bb2`, `dvt`, `pvt`
:SoC: SiFli SF32LB52
:Platform: Emery
:Emulator: `qemu_emery`
:::

Pebble Time 2 is a rectangular color watch with a touchscreen and a heart
rate monitor, built around the SiFli SF32LB52. Its firmware targets the
Emery platform, which apps see as a 200 × 228 color display.

## Hardware

- **SoC**: [SiFli SF32LB52](https://www.sifli.com/en/sf32lb52x), with an Arm STAR-MC1 (Armv8-M) application
  core with FPU, a Bluetooth Low Energy controller on its own core, and
  512 KiB SRAM
- **Flash**: [GigaDevice GD25Q256E](https://www.gigadevice.com/product/flash/spi-nor-flash/gd25q256e), 32 MiB NOR
- **Display**: 200 × 228, 64-color JDI memory-in-pixel, driven by the
  SF32LB52 LCD controller
- **Backlight**: Awinic AW2016 LED driver
- **Touch**: Hynitron CST816 capacitive touch controller
- **Accelerometer and gyroscope**: [ST LSM6DSO](https://www.st.com/en/mems-and-sensors/lsm6dso.html)
- **Magnetometer**: [MEMSIC MMC5603NJ](https://www.memsic.com/magnetometer-2)
- **Heart rate**: Goodix GH3026 optical front end
- **Ambient light**: W1160
- **Vibration**: [Awinic AW86225](https://www.awinic.com/en/productDetail/AW86225CSR) haptic driver, with a linear resonant
  actuator
- **Microphone**: PDM, on the SF32LB52 PDM interface
- **Speaker**: on the SF32LB52 audio codec
- **Power**: [Nordic nPM1300](https://www.nordicsemi.com/Products/nPM1300) PMIC and battery charger

### Revisions

| Revision | Description |
| --- | --- |
| `bb2` | Bring-up board (big board) |
| `dvt` | Design validation units |
| `pvt` | Production validation units |

## Connecting a programmer

Pebble Time 2 is programmed with a {doc}`../../programmers/pblprog_sifli`, over
the watch's debug UART:

1. Connect the programmer's flex cable to the B2B connector on the watch
   mainboard.
2. Connect the programmer to the host over USB-C.
3. Turn the `CHG` switch on to power the watch from USB while programming,
   so it stays reachable whatever its battery level.

The programmer's page describes its connector, reset and charge switch.

Once connected, the {doc}`pbl commands <../../pbl/index>` flash the watch
and reach its console.
