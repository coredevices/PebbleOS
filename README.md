# PebbleOS

This repository contains the source code of PebbleOS.

## How PebbleOS works

Read more in the very detailed [PebbleOS architecture presentation](https://docs.google.com/presentation/d/1wfyBRwbrv5YtSnvNRnEPz5tRx9y7VGcFsuHbi1X-D7I/edit?usp=sharing)

Then read this [presentation on how PebbleOS works](https://docs.google.com/presentation/d/1M--yoEJBO-uckvY5CTFfHT4srw6RCj9RTGT57RcogX8/edit?usp=sharing)!

Join the [Rebble Discord](https://discordapp.com/invite/aRUAYFN) #firmware-dev channel to discuss.

### Watch the Podcast

Several original PebbleOS firmware engineers [recorded a podcast](https://www.youtube.com/watch?v=dk5wsNN8abo) about the OS.
| Podcast | Gemini Summary |
|----------|----------|
| [![CleanShot 2025-02-11 at 22 26 27@2x](https://github.com/user-attachments/assets/9c55aefa-06f5-4a58-bf4f-fa40e1bd45bd)](https://www.youtube.com/watch?v=dk5wsNN8abo) | [![Arc 2025-02-11 22 25 13](https://github.com/user-attachments/assets/ee5361b3-a89c-450e-97a5-f10796c1fba5)](https://g.co/gemini/share/03350ab7b4e6) |

### Architecture

![CleanShot 2025-01-31 at 14 38 16@2x](https://github.com/user-attachments/assets/23d13a36-55e6-4e3a-87ab-4fb1fd1fca5a)

![Arc 2025-01-31 11 44 47](https://github.com/user-attachments/assets/804bc6b9-47c1-4af5-b698-6078aca467ee)

**WARNING**: Codebase is being refactored/modernized, so only certain features
may work right now.

## Getting Started

- Use Linux (tested: Ubuntu 24.04, Fedora 41) or macOS (tested: Sequoia 15.2)
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
- (If you want to use an nRF based board) Install `nrfjprog` from
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
- Install emscripten
  - If you're on Mac and using [Homebrew](https://brew.sh), you can run `brew install emscripten`.
  - If you're on Linux, follow the instructions [here](https://github.com/emscripten-core/emsdk) and install version 4.0.1.
  - You can skip this if you wish by configuring with `--nojs` but beware the built-in clock for several devices requires JS and will render a blank screen when disabled.

## Building

First, configure the project like this:

```shell
./waf configure --board <board>
```

Note: If you wish to debug, you're likely to want `--nowatchdog --nostop --nosleep` also.

At this moment, only the following boards are supported/tested:

- asterix_evt1
- snowy_bb2 (no Bluetooth)
- silk_bb2 (no Bluetooth)

Then build:

```shell
./waf build
```

PRF can be also be built:

```shell
./waf build_prf
```

## Flashing firmware (silk/snowy bigboards)

Make sure openocd is installed, then run:

```shell
./waf flash
```

If bootloader image is not available, or you simply want to flash the firmware
image, use:

```shell
./waf flash_fw
```

Note that you may change the default probe using the `--jtag` option when
configuring the project.

## Flashing firmware (nRF)

First make sure Nordic S140 Softdevice is flashed (only do this once):

```shell
nrfjprog --program src/fw/vendor/nrf5-sdk/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectoranduicrerase --reset
```

Flash the firmware:

```shell
nrfjprog --program build/src/fw/tintin_fw.elf --sectorerase --reset
```

## Flashing resources

The first time you boot you may see a "sad watch" screen because resources are not
flashed. To flash resources, run:

```shell
./waf image_resources
```

You don't need to do this every time, only when resources are changed.

## Viewing logs

```shell
./waf console
```

## Building for QEMU

The same QEMU binary found in the SDK can be used to build and develop the firmware.

If you're using an Apple Silicon Mac, you might find it easier to build QEMU from [source](https://github.com/pebble-dev/qemu).

The steps here are similar that of real hardware:

```shell
./waf configure --board=snowy_bb2 --qemu
./waf build
./waf qemu_image_spi
./waf qemu
```

You can then launch a console using:

```shell
./waf qemu_console
```

Finally, you can debug with GDB using:

```shell
./waf qemu_gdb
```
