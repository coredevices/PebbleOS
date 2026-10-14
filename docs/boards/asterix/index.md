# Pebble 2 Duo (asterix)

## Overview

:::{div} board-card

```{image} images/asterix.webp
:alt: Pebble 2 Duo
```

:Board: `asterix`
:SoC: Nordic nRF52840
:Platform: Flint
:Emulator: `qemu_flint`
:::

Pebble 2 Duo, announced as Core 2 Duo, is a rectangular black and white
watch built around the Nordic nRF52840. Its firmware targets the Flint
platform, which apps see as a 144 × 168 black and white display.

## Hardware

- Nordic nRF52840: Arm Cortex-M4 with FPU and an integrated Bluetooth Low
  Energy radio
- 1 MiB internal flash, 256 KiB RAM, 32 MiB external NOR flash
  (GigaDevice GD25LQ255E)
- 144 × 168 Sharp memory LCD with a backlight
- Accelerometer and gyroscope, magnetometer, barometric pressure sensor,
  ambient light sensor
- Microphone and speaker
- Vibration motor
- Nordic nPM1300 PMIC and battery charger

### Supported features

| Interface | Hardware | Kconfig |
| --- | --- | --- |
| Display | Sharp LS013B7DH01 | `CONFIG_DISPLAY_SHARP_LS013B7DH01_NRF5` |
| Backlight | PWM | `CONFIG_BACKLIGHT_PWM` |
| Accelerometer | ST LSM6DSO | `CONFIG_ACCEL_LSM6DSO` |
| Magnetometer | MEMSIC MMC5603NJ | `CONFIG_MAG_MMC5603NJ` |
| Pressure | Bosch BMP390 | `CONFIG_PRESSURE_BMP390` |
| Ambient light | TI OPT3001 | `CONFIG_AMBIENT_LIGHT_OPT3001` |
| Vibration | TI DRV2604 | `CONFIG_VIBE_DRV2604` |
| Microphone | nRF52840 PDM | `CONFIG_MIC_NRF5` |
| Speaker | Dialog DA7212 codec | `CONFIG_SPEAKER_DA7212` |
| PMIC, battery | Nordic nPM1300 | `CONFIG_MFD_NPM1300` |
| Flash | GigaDevice GD25LQ255E | `CONFIG_FLASH_GD25LQ255E` |

The defconfig in `boards/asterix/` enables the rest of the configuration;
{doc}`../../pbl/menuconfig` shows the full set for a build.

## Programming and debugging

### Building

```shell
pbl configure --board asterix
pbl build
```

### Flashing

Pebble 2 Duo is flashed through a firmware development kit, described
below, with the `openocd` {ref}`runner <pbl-runners>` (the default) or
`nrfutil`:

```shell
pbl flash
pbl flash -r nrfutil
```

The first time, write the system resources once the firmware is running,
over the kit's serial port:

```shell
pbl image_resources --tty /dev/ttyACM0
```

Without a development kit, install a {doc}`../../pbl/bundle` from the
Pebble mobile app instead.

### Console

```shell
pbl console --tty /dev/ttyACM0
```

### Debugging

The development kit's CMSIS-DAP probe gives full debugging through OpenOCD:

```shell
pbl debug
```

{doc}`../../development/debugging` describes the GDB extensions the
firmware ships with.

### Recovery firmware

Build the recovery firmware (PRF) with `--variant prf`:

```shell
pbl -b build-prf configure --board asterix --variant prf
pbl -b build-prf build
```

### Firmware development kit

The Asterix/Pebble 2 Duo (announced as Core 2 Duo) mainboard has a B2B (Board-To-Board) connector that gives access to:

- MCU VDD, VUSB and GND
- MCU SWCLK, SWDIO and RESET
- Debug UART RX/TX

The connector part number is Molex 5050040812, and the pinout is as shown below.

```{figure} images/b2b-pinout.webp
Asterix B2B connector pinout
```

The "Core B2B v2" board has been designed as a companion programming board.
It is based on the [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html).
Software for the `Core B2B v2` board is available in the [Core B2B v2 repository](https://github.com/coredevices/asterix_b2b2_fw).
Below you can find a picture of how it is connected, and a list of its main features.

```{figure} images/asterix-programming.webp
Asterix and Core B2B v2
```

```{warning}
External connectors (SWD/UART) expose 1.8V signals.
Do not use any adapter that does not operate at 1.8V, or there is a **risk of damage**!
```

1. Asterix B2B connector
2. USB connector

   - Powers the board
   - Provides VUSB
   - Exposes a CMSIS-DAP device and a virtual COM port

3. Debug UART routing

   - L: External connector (5)
   - R: Embedded virtual COM port (2)

4. SWD routing

   - L: External SWD connector (6)
   - R: Embedded CMSIS-DAP (2)

5. Debug UART pins
6. External SWD connector
7. MCU Reset
8. VUSB switch

   - L: connected
   - R: disconnected

#### Usage and tips

After receiving your Firmware Dev Kit, please update the firmware on the board to the [latest release](https://github.com/coredevices/asterix_b2b2_fw/releases).

When using the built-in programmer and USB-UART converter, make sure to place switches (3) and (4) to the right, as shown in the picture below.
It is also recommended to provide VUSB while programming so that the watch is always programmable regardless of the battery level.
For this to happen, place switch (8) to the left.

```{figure} images/b2bv2-switch-orientation.webp
Correct orientation of the switches when using built-in programmer, USB-UART converter and powering VUSB
```

- Please make sure to connect the B2Bv2 board in the **EXACT** way detailed in this document.
  If you connect it in any other orientation, your watch will be rendered unusable.
  You have been warned.
- If you are having trouble programming your watch, check the orientation of the switches.
  Sometimes they accidentally get switched to the wrong side during shipping.
- If your serial console is showing text but not accepting input, then your connector is likely not seated correctly.
  Press to click it in more.
