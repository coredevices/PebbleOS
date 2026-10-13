# `pbl touch`

Tap the running emulator's screen, on touch-capable boards. Coordinates are
screen pixels; the display size is read from the emulated `pebble-touch`
device and scaled automatically. A tap is a finger down then up.

```shell
pbl touch 130 130
```

Injection goes through the QMP socket {doc}`qemu` opens, on the
absolute-pointer input path; multi-touch is not wired up in the device. See
{doc}`swipe` for drags.

## Options

```{pbl-command} touch
```
