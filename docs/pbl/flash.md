# `pbl flash`

Flash the firmware to a watch connected through a
{doc}`programmer <../programmers/index>`, using the board's
{ref}`runner <pbl-runners>`.

```shell
pbl flash
```

Before flashing, check the {doc}`board's page <../boards/index>` for how to
connect the programmer. The `sftool` runner talks to the watch over
a serial port, given with `--tty`, e.g. `/dev/ttyACM0` or
`/dev/tty.usbmodem1102`:

```shell
pbl flash --tty /dev/ttyACM0
```

The first time, the system resources have to be written too. Boards using
`sftool` take `--resources`; on the others, image them over the serial
console once the firmware is running, with {doc}`image_resources`.

When both firmware and resources are written, the watch boots into the
main application; {doc}`console` shows its logs.

A watch without a programmer can only be updated by sideloading a
{doc}`bundle`.

## Options

```{pbl-command} flash
```
