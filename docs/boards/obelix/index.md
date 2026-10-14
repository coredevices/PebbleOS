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

- SiFli SF32LB52: Arm STAR-MC1 (Armv8-M) application core with FPU, and a
  Bluetooth Low Energy controller on its own core
- 512 KiB SRAM, 32 MiB external NOR flash (GigaDevice GD25Q256E)
- 200 × 228, 64-color JDI memory-in-pixel display with a backlight
- Capacitive touch panel
- Heart rate sensor, accelerometer and gyroscope, magnetometer, ambient
  light sensor
- Microphone and speaker
- Linear resonant actuator for vibration
- Nordic nPM1300 PMIC and battery charger

### Supported features

| Interface | Hardware | Kconfig |
| --- | --- | --- |
| Display | JDI memory-in-pixel (SF32LB52 LCDC) | `CONFIG_DISPLAY_JDI_SF32LB` |
| Backlight | Awinic AW2016 | `CONFIG_BACKLIGHT_AW2016` |
| Touch | Hynitron CST816 | `CONFIG_TOUCH_CST816` |
| Accelerometer | ST LSM6DSO | `CONFIG_ACCEL_LSM6DSO` |
| Magnetometer | MEMSIC MMC5603NJ | `CONFIG_MAG_MMC5603NJ` |
| Heart rate | Goodix GH3X2X | `CONFIG_HRM_GH3X2X` |
| Ambient light | W1160 | `CONFIG_AMBIENT_LIGHT_W1160` |
| Vibration | Awinic AW86225 | `CONFIG_VIBE_AW86225` |
| Microphone | SF32LB52 PDM | `CONFIG_MIC_SF32LB` |
| Speaker | SF32LB52 audio codec | `CONFIG_SPEAKER_SF32LB` |
| PMIC, battery | Nordic nPM1300 | `CONFIG_MFD_NPM1300` |
| Flash | GigaDevice GD25Q256E | `CONFIG_FLASH_GD25Q256E` |

The defconfigs in `boards/obelix/` enable the rest of the configuration;
{doc}`../../pbl/menuconfig` shows the full set for a build.

### Revisions

| Revision | Description |
| --- | --- |
| `bb2` | Bring-up board (big board) |
| `dvt` | Design validation units |
| `pvt` | Production validation units |

## Programming and debugging

### Building

```shell
pbl configure --board obelix@pvt
pbl build
```

### Flashing

Pebble Time 2 is flashed with the `sftool` {ref}`runner <pbl-runners>`,
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
`qemu_emery`.

### Recovery firmware

The board boots through PebbleOS's own bootloader, which falls back to the
recovery firmware (PRF) when the main firmware does not start. Build the
PRF with `--variant prf`:

```shell
pbl -b build-prf configure --board obelix@pvt --variant prf
pbl -b build-prf build
```
