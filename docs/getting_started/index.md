# Getting started guide

This guide takes you from zero to PebbleOS booting: from a fresh machine to
the firmware running in the emulator, then to the same firmware installed on
a watch.

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

The [PebbleOS SDK](https://github.com/coredevices/PebbleOS-SDK) bundles the
ARM GNU toolchain, Pebble QEMU, sftool and the other tools the build runs:

```shell
curl -LsSf https://github.com/coredevices/PebbleOS-SDK/releases/latest/download/pebbleos-sdk-installer.sh | sh
```

The build finds the SDK on its own: it honours `PEBBLEOS_SDK_ROOT` if set
(the SDK's `env.sh` exports it), and otherwise picks the newest
`pebbleos-sdk-<version>` under your home directory or `/opt` that satisfies
the version in `SDK_VERSION`. The SDK and the tools found in it are cached
in the build directory, so a build keeps using the SDK it was configured
with; pass `-DPEBBLEOS_SDK_ROOT=<dir>` to `pbl configure` to pick a
specific one.

## Build for the emulator

Configure a build directory for one of the QEMU boards and build it.
`qemu_emery` targets the Emery platform (Pebble Time 2); `qemu_flint` and
`qemu_gabbro` target Pebble 2 Duo and Pebble Round 2:

```shell
pbl configure --board qemu_emery
pbl build
```

{doc}`../pbl/configure` runs CMake into `build/` and remembers the board,
so later commands do not need it again.

## Run it in QEMU

```shell
pbl qemu
```

A window opens with the emulated watch and the firmware boots to the
watchface. The terminal that launched it is the QEMU monitor. The arrow keys
are the watch buttons: `left` is back, `right` is select, `up` and `down`
are up and down. Quit with `quit` at the monitor prompt or by closing the
window.

With the emulator running, other shells can attach to it:

```shell
pbl console       # the firmware's debug console
pbl screenshot    # writes build/screenshot.png
```

## Install an app on the emulator

With QEMU running and the [`pebble` tool](https://github.com/coredevices/pebble-tool)
on your `PATH`, install a `.pbw` from another shell:

```shell
pebble install /path/to/your/file.pbw --qemu
```

The app starts on the emulated watch. Pass `--keep-flash-image` to
`pbl qemu` on later launches to keep installed apps.

## Build for a watch

Configure a second build directory for a real board. `asterix` is Pebble 2
Duo; the other boards and their revisions are listed in {doc}`../boards/index`:

```shell
pbl -b build-asterix configure --board asterix
pbl -b build-asterix build
pbl -b build-asterix bundle
```

{doc}`../pbl/bundle` writes a `.pbz` into the build directory, named
`normal_<board>_<version>.pbz`.

## Install it on the watch

There are two ways to get the firmware onto a watch:

1. **Sideload the bundle.** Copy the `.pbz` to the phone paired with the
   watch. In the Pebble mobile app, enable `Settings -> Show debug
   options`, then go back to the Devices tab, tap your watch, choose
   `Firmware Update Debug -> Sideload FW`, and select the file. The app
   starts the update and shows its progress on the watch card.
   {doc}`../pbl/bundle` also covers sideloading over adb and with the
   `pebble` tool.
2. **Flash it with a programmer.** With a
   {doc}`programmer <../programmers/index>` connected,
   {doc}`../pbl/flash` writes the firmware directly, without the phone:

   ```shell
   pbl -b build-asterix flash
   ```

```{note}
Holding Back + Up + Select reboots the watch into the recovery firmware
(PRF), from which the Pebble mobile app can install a firmware again.
```

## Next steps

- {doc}`../pbl/index` documents every `pbl` command.
- {doc}`../development/emulator` explains what the emulator models and how
  to operate it.
- {doc}`../development/testing` and {doc}`../development/integration_tests`
  run the test suites.
- {doc}`../architecture/index` explains how the firmware is put together.
