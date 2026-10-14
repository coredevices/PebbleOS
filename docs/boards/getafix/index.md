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

- SiFli SF32LB52: Arm STAR-MC1 (Armv8-M) application core with FPU, and a
  Bluetooth Low Energy controller on its own core
- 512 KiB SRAM, 32 MiB external NOR flash (GigaDevice GD25Q256E)
- 260 × 260, 64-color round JDI memory-in-pixel display with a backlight
- Capacitive touch panel
- Accelerometer, magnetometer, ambient light sensor
- Microphone
- Linear resonant actuator for vibration
- Nordic nPM1300 PMIC and battery charger

### Supported features

| Interface | Hardware | Kconfig |
| --- | --- | --- |
| Display | JDI memory-in-pixel (SF32LB52 LCDC) | `CONFIG_DISPLAY_JDI_SF32LB` |
| Backlight | Awinic AW9364E | `CONFIG_BACKLIGHT_AW9364E` |
| Touch | Hynitron CST816 | `CONFIG_TOUCH_CST816` |
| Accelerometer | ST LIS2DW12 | `CONFIG_ACCEL_LIS2DW12` |
| Magnetometer | MEMSIC MMC5603NJ | `CONFIG_MAG_MMC5603NJ` |
| Ambient light | W1160 | `CONFIG_AMBIENT_LIGHT_W1160` |
| Vibration | Awinic AW86225 (`dvt2`), AW8623X (`dvt`) | `CONFIG_VIBE_AW86225`, `CONFIG_VIBE_AW8623X` |
| Microphone | SF32LB52 PDM | `CONFIG_MIC_SF32LB` |
| PMIC, battery | Nordic nPM1300 | `CONFIG_MFD_NPM1300` |
| Flash | GigaDevice GD25Q256E | `CONFIG_FLASH_GD25Q256E` |

The defconfigs in `boards/getafix/` enable the rest of the configuration;
{doc}`../../pbl/menuconfig` shows the full set for a build.

### Revisions

| Revision | Description |
| --- | --- |
| `dvt` | Design validation units, with an AW8623X haptic driver |
| `dvt2` | Second design validation build, with an AW86225 haptic driver |

## Programming and debugging

### Building

```shell
pbl configure --board getafix@dvt2
pbl build
```

### Flashing

Pebble Round 2 is flashed with the `sftool` {ref}`runner <pbl-runners>`,
over the serial port of a firmware development kit. The first time, write
the system resources too:

```shell
pbl flash --tty /dev/ttyACM0 --resources
```

Later updates can leave `--resources` out. Without a development kit,
install a {doc}`../../pbl/bundle` from the Pebble mobile app instead.

### Console

```shell
pbl console --tty /dev/ttyACM0
```

### Debugging

`pbl debug` is not available on this board, as `sftool` has no debug
server. Use the console, the flash log buffer and coredumps described in
{doc}`../../development/debugging`, or reproduce the problem on
`qemu_gabbro`.

### Recovery firmware

The board boots through PebbleOS's own bootloader, which falls back to the
recovery firmware (PRF) when the main firmware does not start. Build the
PRF with `--variant prf`:

```shell
pbl -b build-prf configure --board getafix@dvt2 --variant prf
pbl -b build-prf build
```
