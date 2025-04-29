# QEMU

```{important}
QEMU is only available for STM32 targets
```

## Getting QEMU

The same QEMU binary found in the SDK can be used to build and develop the firmware.
Below you can also find a detailed guide on how to build it from source if you need to do so (e.g. on Apple Silicon).

### Building from source

1. Install OS-level pre-requisites:

:::::{tab-set}
:sync-group: os

::::{tab-item} Ubuntu 24.04 LTS
:sync: ubuntu

```shell
sudo apt install autoconf libglib2.0-dev libpixman-1-dev
```

::::

::::{tab-item} macOS
:sync: macos

```shell
brew install autoconf glib pixman
```

::::
:::::

2. Install `pyenv` following [this guide](https://github.com/pyenv/pyenv?tab=readme-ov-file#installation) (steps A-D).
3. Install Python 2.7:

```shell
pyenv install 2.7
```

4. Activate Python 2.7 on the current shell:

```shell
pyenv local 2.7
```

5. Clone QEMU

```shell
git clone --recurse-submodules https://github.com/pebble-dev/qemu
cd qemu
```

6. Configure QEMU:

```shell
./configure \
  --disable-werror \
  --enable-debug \
  --target-list="arm-softmmu" \
  --extra-cflags=-DSTM32_UART_NO_BAUD_DELAY
```

7. Build QEMU:

```shell
make
```

8. Make sure to make it available on your `PATH`:

```shell
export PATH=$PWD/arm-softmmu:$PATH
```

## Build

The steps here are similar that of real hardware:

```shell
./waf configure --board=$BOARD --qemu
./waf build
./waf qemu_image_spi
```

where `$BOARD` is any STM32 based board.

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
