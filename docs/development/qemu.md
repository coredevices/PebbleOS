# QEMU

```{important}
Some platforms have dedicated QEMU board targets: `qemu_emery`, `qemu_flint`,
and `qemu_gabbro`. Some other boards can also be built for QEMU using the
`--qemu` flag.
```

## Getting QEMU

The same QEMU binary found in the SDK can be used to build and develop the firmware.
Prebuilt binaries are also published on the
[`coredevices/qemu` releases page](https://github.com/coredevices/qemu/releases)
for Linux and macOS (x86_64 and arm64). Download the archive matching your
platform, extract it, and add the extracted directory to your `PATH`.

If you need to build QEMU yourself (e.g. to make local changes), follow the
guide below.

### Building from source

1. Install OS-level pre-requisites:

:::::{tab-set}
:sync-group: os

::::{tab-item} Ubuntu 24.04 LTS
:sync: ubuntu

```shell
sudo apt install libsdl2-dev libpixman-1-dev libglib2.0-dev \
                 pkg-config ninja-build python3-venv build-essential
```

::::

::::{tab-item} macOS
:sync: macos

```shell
brew install sdl2 pixman glib pkg-config ninja
```

::::
:::::

QEMU also requires Python ≥ 3.9. Recent Ubuntu and macOS releases ship a
suitable interpreter; if yours does not, install one via your package
manager or `pyenv` before continuing.

2. Clone QEMU:

```shell
git clone https://github.com/coredevices/qemu
cd qemu
```

3. Configure QEMU (out-of-tree builds are required):

```shell
mkdir build && cd build
../configure \
  --disable-werror \
  --enable-debug \
  --target-list=arm-softmmu
```

4. Build QEMU:

```shell
make
```

5. Point the firmware build at your `qemu-system-arm` binary. Either add it
   to your `PATH`:

   ```shell
   export PATH=$PWD:$PATH
   ```

   or, if you'd rather not shadow another `qemu-system-arm` already on your
   `PATH`, set `PEBBLE_QEMU_BIN` to the binary's absolute path:

   ```shell
   export PEBBLE_QEMU_BIN=$PWD/qemu-system-arm
   ```

## Build

The steps here are similar that of real hardware:

```shell
./waf configure --board=$BOARD
./waf build
./waf qemu_image_spi
```

where `$BOARD` is one of the dedicated QEMU boards (`qemu_emery`,
`qemu_flint`, `qemu_gabbro`). For boards without a dedicated QEMU
target, pass `--qemu` to the configure step instead:

```shell
./waf configure --board=$BOARD --qemu
```

## Run

You can launch QEMU with the built image using:

```shell
./waf qemu
```

## Console

You can launch a console using:

```shell
./waf qemu_console
```

## Debug

You can debug with GDB using:

```shell
./waf qemu_gdb
```
