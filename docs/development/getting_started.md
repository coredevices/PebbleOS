# Prerequisites

Follow this guide to:

- Set up a command-line PebbleOS development environment
- Get the source code

## ARM toolchain + system-level dependencies

First, download the Arm GNU toolchain `arm-none-eabi` 14.2.Rel1 from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).
Make sure to make it available on your path `PATH` and then check GCC version is reported correctly:

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
sudo apt install bison clang flex gcc gcc-multilib gperf git gettext libfreetype6-dev libglib2.0-dev libgtk-3-dev libncurses-dev librsvg2-bin make python3-dev python3-venv iopenocd
```

::::

::::{tab-item} macOS
:sync: macos

1. Install [brew](https://brew.sh/).

2. Install dependencies:

```shell
brew install librsvg python openocd
```

3. Link `brew` Python:

```shell
brew link python@3
```

::::

:::::

## Get the source code

You can clone the PebbleOS repository by running:

```shell
git clone --recurse-submodules https://github.com/coredevices/pebbleos
```

Once cloned, enter the `pebbleos` directory before continuing:

```shell
cd pebbleos
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

