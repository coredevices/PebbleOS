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
./pbl configure --board=$BOARD
./pbl build
```

where `$BOARD` is one of the dedicated QEMU boards (`qemu_emery`,
`qemu_flint`, `qemu_gabbro`).

## Run

You can launch QEMU with the built image using:

```shell
./pbl qemu
```

The flash image is rebuilt by default on every launch. To keep the existing flash image (e.g. to preserve stored apps), pass `--keep-flash-image`:

```shell
./pbl qemu --keep-flash-image
```

## Console

You can launch a console using:

```shell
./pbl console
```

## Debug

You can debug with GDB using:

```shell
./pbl debug
```
