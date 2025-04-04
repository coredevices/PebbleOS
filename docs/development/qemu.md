# QEMU

```{important}
QEMU is only available for STM32 targets
```

## Getting QEMU

The same QEMU binary found in the SDK can be used to build and develop the firmware.
If you're using an Apple Silicon Mac, you might find it easier to build QEMU from [source](https://github.com/pebble-dev/qemu).

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
