# Getting Started

NOTE: This has been tested on Fedora Linux 41, Ubuntu 24.04 and macOS Sequoia
15.2.

## Pre-requisites

- Clone the submodules:
  ```shell
  git submodule init
  git submodule update
  ```
- Install GNU ARM Embedded toolchain from
  https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads. Make
  sure it is available on your `PATH` by checking `arm-none-eabi-gcc --version`
  returns the expected version.
- If using Ubuntu, install `gcc-multilib` and `gettext`
- Install `nrfjprog` from
  https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools.
- Create a Python venv:

  ```shell
  python -m venv .venv
  ```
- Activate the Python venv (also every time you start working):
  ```shell
  source .venv/bin/activate
  ```
- Install dependencies:
  ```shell
  pip install -r requirements-linux.txt
  ```
- Install local dependencies:
  ```shell
  pip install -e \
    python_libs/pblprog \
    python_libs/pebble-commander \
    python_libs/pulse2 \
    python_libs/pebble-loghash
  ```

## Building

First, configure the project like this:

```shell
./waf configure --board asterix_vla_dvb1 --nojs --nohash
```

Then build:

```shell
./waf build
```

PRF can be also be built:

```shell
./waf build_prf
```

## Flashing

First make sure Nordic S140 Softdevice is flashed (only do this once):

```shell
nrfjprog --program src/fw/vendor/nrf5-sdk/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectoranduicrerase --reset
```

Flash the firmware:

```shell
nrfjprog --program build/src/fw/tintin_fw.elf --sectorerase --reset
```

First time you should see a "sad watch" screen because resources are not
flashed. To flash resources, run:

```shell
python tools/pulse_flash_imaging.py -t /dev/$PORT -p resources build/system_resources.pbpack
```

## Viewing logs

```shell
python tools/pulse_console.py -t /dev/$PORT
```
