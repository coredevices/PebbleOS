# Getting started guide

This guide takes you from a fresh machine to PebbleOS running, in the
emulator or on a watch.

## Install dependencies

Install the packages the build needs from your system's package manager.

:::::{tab-set}
:sync-group: os

::::{tab-item} Ubuntu
:sync: ubuntu

Ubuntu 24.04 LTS or later:

```shell
sudo apt update
sudo apt install \
    bison \
    clang \
    cmake \
    flex \
    gcc \
    gcc-multilib \
    gettext \
    git \
    gperf \
    libfreetype6-dev \
    libglib2.0-dev \
    libgtk-3-dev \
    libncurses-dev \
    librsvg2-bin \
    libsdl2-dev \
    make \
    nodejs \
    openocd \
    pkg-config \
    python3-dev \
    python3-venv
```

::::

::::{tab-item} Fedora
:sync: fedora

Fedora 44 or later:

```shell
sudo dnf upgrade --refresh
sudo dnf install \
    bison \
    clang \
    cmake \
    dash \
    flex \
    freetype-devel \
    gcc \
    gettext \
    git \
    glib2-devel \
    gperf \
    gtk3-devel \
    librsvg2-tools \
    make \
    ncurses-devel \
    nodejs \
    openocd \
    pkgconf-pkg-config \
    python3-devel \
    SDL2-devel
```

::::

::::{tab-item} macOS
:sync: macos

Install [Homebrew](https://brew.sh/), then Python, Git and OpenOCD:

```shell
brew install python git openocd
brew link python@3
```

The remaining packages are listed in the repository and installed once it
is cloned, in the next step.

::::

:::::

## Get the source code

Clone the repository with its submodules and enter it:

```shell
git clone --recurse-submodules https://github.com/coredevices/pebbleos
cd pebbleos
```

:::::{tab-set}
:sync-group: os

::::{tab-item} Ubuntu
:sync: ubuntu

Nothing else to install.

::::

::::{tab-item} Fedora
:sync: fedora

Nothing else to install.

::::

::::{tab-item} macOS
:sync: macos

Install the remaining packages:

```shell
brew install $(cat requirements-brew.txt)
```

::::

:::::

## Install Python dependencies

Create a [virtual environment](https://docs.python.org/3/library/venv.html)
in the checkout, activate it and install the Python dependencies:

```shell
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

This also installs {doc}`pbl <../pbl/index>`, the developer command line the
rest of this guide uses.

```{tip}
Activate the virtual environment (`source .venv/bin/activate`) in every new
shell before running `pbl`.
```

## Install the PebbleOS SDK

The [PebbleOS SDK](https://github.com/coredevices/PebbleOS-SDK) provides
the toolchain and tools the build needs:

```shell
curl -LsSf https://github.com/coredevices/PebbleOS-SDK/releases/latest/download/pebbleos-sdk-installer.sh | sh
```

The build finds the installed SDK on its own;
{doc}`../development/build_system/index` explains how to pick a specific
one.

## Build for a board

Every build targets a board, either an emulated watch such as `qemu_emery`
or a real one; {doc}`../boards/index` lists them. Configure a build
directory for one and build it:

```shell
pbl configure --board qemu_emery
pbl build
```

## Run the firmware

```shell
pbl run
```

On an emulated board, a window opens with the emulated watch and the
firmware boots to the watchface; the arrow keys are the watch buttons. The
{doc}`../development/emulation/emulator` page covers everything else you
can do with it.

On a real board, `pbl run` flashes the firmware to the watch. With a
{doc}`programmer <../programmers/index>` connected as the
{doc}`board's page <../boards/index>` shows, add `--resources` the first
time to write the system resources too. Without one, go through the phone
paired with the watch: enable the Developer Connection in the Pebble app
and pass the address it shows:

```shell
pbl run -r phone --address $IP
```

{doc}`../pbl/run` covers the options of each case.

## Next steps

- {doc}`../pbl/index` documents every `pbl` command.
- {doc}`../development/emulation/index` compares the emulator and the
  native simulator.
- {doc}`../development/tests/index` covers the unit and integration test
  suites.
- {doc}`../architecture/index` explains how the firmware is put together.
