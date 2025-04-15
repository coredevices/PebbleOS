# 🚀 Getting Started

Follow this guide to:

- Set up a command-line PebbleOS development environment
- Get the source code
- Build, flash, and run PebbleOS on a watch with programming port access

## Pre-requisites

First download the Arm GNU toolchain `arm-none-eabi` 14.2.Rel1 from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).
Make sure to make it available on your path `PATH` by checking GCC version:

```shell
$ arm-none-eabi-gcc --version
arm-none-eabi-gcc (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119
Copyright (C) 2024 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

A series of system-level dependencies are required.
Follow the next steps to install them.

:::::{tab-set}
:sync-group: os

::::{tab-item} Ubuntu 24.04 LTS
:sync: ubuntu

1. Update package list:

```shell
sudo apt update
```

2. Install required dependencies

```shell
sudo apt install clang gcc gcc-multilib git gettext python3-dev python3-venv
```

::::

::::{tab-item} macOS
:sync: macos

1. Install [brew](https://brew.sh/).

::::

:::::

If building with Javascript support enabled (default), install Emscripten:

:::::{tab-set}
:sync-group: os

::::{tab-item} Ubuntu 24.04 LTS
:sync: ubuntu

1. Install Emscripten SDK as detailed [here](https://emscripten.org/docs/getting_started/downloads.html).
   Pick version `4.0.1` instead of `latest` when running `./emsdk install` or `./emsdk activate`.
   To conveniently access Emscripten SDK tools, the activate command will offer some suggestions.
   It is recommended to follow them.
::::

::::{tab-item} macOS
:sync: macos

1. Install Emscripten using `brew`:

```shell
brew install emscripten@4.0.1
```

::::
:::::

## Get the source code

You can clone the PebbleOS repository by running:

```shell
git clone --recurse-submodules https://github.com/pebble-dev/pebble-firmware
```

Once cloned, enter the `pebble-firmware` directory before continuing:

```shell
cd pebble-firmware
```

## Python dependencies

A series of additional Python dependencies are also required.
Follow the next steps to install them in a [Python virtual environment](https://docs.python.org/3/library/venv.html).

1. Create a new virtual environment:

```shell
python3 -m venv .venv
```

2. Activate the virtual environment:

```shell
source .venv/bin/activate
```

```{tip}
Remember to activate the virtual environment before every time you start working!
```

3. Install dependencies

```shell
pip install -r requirements.txt
```

## Building

1. Configure the project:

```shell
./waf configure --board $BOARD
```

where `$BOARD` is any of the supported boards, e.g. `snowy_bb2`, `asterix_evt1`, ...

2. Build:

```shell
./waf build
```

## Flashing

You can flash the built firmware (including pre-compiled bootloader) by running:

```shell
./waf flash
```

If flashing for the first time, your watch will boot into a _sad watch_ state, indicating that resources need to be flashed:

```shell
./waf image_resources
```

You will need to add `--tty /path/to/serial/adapter` if not using a big board with built-in FTDI.

At this point you should observe the watch booting into the main application.
You can also see the logs by opening the console:

```shell
./waf console
```

Similarly, append `--tty` if needed.
