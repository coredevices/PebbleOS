# QEMU

```{important}
Some platforms have dedicated QEMU board targets: `qemu_emery`, `qemu_flint`,
and `qemu_gabbro`.
```

## Getting QEMU

The QEMU binary ships with the [PebbleOS SDK](https://github.com/coredevices/PebbleOS-SDK).

## Build

The steps here are similar that of real hardware:

```shell
./waf configure --board=$BOARD
./waf build
```

where `$BOARD` is one of the dedicated QEMU boards (`qemu_emery`,
`qemu_flint`, `qemu_gabbro`).

## Run

You can launch QEMU with the built image using:

```shell
./waf qemu
```

The flash image is built automatically the first time. To force rebuilding it, pass `--new-flash-image`:

```shell
./waf qemu --new-flash-image
```

## Console

You can launch a console using:

```shell
./waf console
```

## Debug

You can debug with GDB using:

```shell
./waf debug
```
