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

The first time, the system resources have to be written too. The `sftool`
runner, and the `openocd` and `nrfutil` ones on asterix, take `--resources`;
otherwise, image them over the serial console once the firmware is running,
with {doc}`image_resources`.

When both firmware and resources are written, the watch boots into the
main application; {doc}`console` shows its logs.

A watch without a programmer can be flashed through its phone with the
`phone` runner. Enable the
[Developer Connection](https://developer.repebble.com/guides/tools-and-resources/developer-connection/)
in the Pebble app and pass the address it shows:

```shell
pbl flash -r phone --address 192.168.1.20
```

The runner reboots the watch into PRF, which invalidates the firmware slots,
then installs the firmware and the system resources; PRF takes both or
neither, so `--resources` is implied. PRF installs into slot 0, so on boards
with two firmware slots the build has to be the slot 0 one, the default.
A watch can also be updated by sideloading a {doc}`bundle`.

## Options

```{pbl-command} flash
```
