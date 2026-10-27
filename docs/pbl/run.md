# `pbl run`

Run the firmware with the board's {ref}`runner <pbl-runners>`: in the
emulator for a QEMU board, as a host program or in a browser for a native
board, or flashed to a watch.

```shell
pbl run
```

Arguments after `--` go to the program the runner starts, e.g. QEMU:

```shell
pbl run -- -d guest_errors
```

## In the emulator

On a QEMU board, the `qemu` runner launches the firmware in the emulator:

```shell
pbl configure --board qemu_emery
pbl build
pbl run
```

The flash image is rebuilt on every launch; `--keep-flash-image` keeps it,
and with it installed apps, data and Bluetooth bondings.

The launched QEMU exposes:

- An interactive QEMU monitor on the launching terminal (`-monitor stdio`)
- A programmatic socket monitor (`-monitor unix:build/qemu-mon.sock`)
- The serial console over TCP on `localhost:12345`, for {doc}`console`
- The Pebble protocol over TCP on `localhost:12344`, for the `pebble` tool
  and {doc}`emulator`

UART1 output is also captured to `uart1.log` in the repository root.
{doc}`../development/emulation/emulator` covers what the emulator models and how to
operate it.

## On the host

On a native board, the `native` runner starts the
{doc}`native simulator <../development/emulation/native_simulator>`, with
its external flash in `pebbleos-flash.bin` in the build directory, kept
across runs, and the Pebble protocol over TCP on `localhost:12344`, as the
emulator, for the `pebble` tool and {doc}`emulator`. Arguments after `--`
are the program's own options, e.g. a window scale factor:

```shell
pbl configure --board native_emery
pbl build
pbl run -- -s 2
```

`--bt-hci` connects the Bluetooth HCI UART of a `CONFIG_BT_HCI_UART` build
to a controller, as in the emulator.

## In a browser

The `wasm` variant of a native board is run by the `wasm` runner: it serves
the build directory on `localhost`, port 8000 or `--http-port`, and opens
the page in a browser, unless `--no-browser` is given. `Ctrl-C` stops it;
reloading the page picks up a rebuild.

```shell
pbl configure --board native_emery/wasm
pbl build
pbl run
```

## On a watch

On a watch, running the firmware is flashing it. With a
{doc}`programmer <../programmers/index>` connected as the
{doc}`board's page <../boards/index>` shows, the board's runner writes it
directly. The `sftool` runner talks to the watch over a serial port, given
with `--tty`, e.g. `/dev/ttyACM0` or `/dev/tty.usbmodem1102`:

```shell
pbl run --tty /dev/ttyACM0
```

The first time, the system resources have to be written too. The `sftool`
runner, and the `openocd` and `nrfutil` ones on asterix, take `--resources`;
otherwise, image them over the serial console once the firmware is running,
with {doc}`image_resources`.

When both firmware and resources are written, the watch boots into the
main application; {doc}`console` shows its logs.

A watch without a programmer is flashed through its phone with the `phone`
runner. An Android phone connected over
[adb](https://developer.android.com/tools/adb) needs nothing else: the
runner starts the Pebble app's
[Developer Connection](https://developer.repebble.com/guides/tools-and-resources/developer-connection/)
itself and reaches it through an adb port forward. Pass `--adb-serial` when
adb sees more than one phone:

```shell
pbl run -r phone
```

Any other phone is reached over the network: enable the Developer
Connection in the Pebble app and pass the address it shows:

```shell
pbl run -r phone --address $IP
```

The runner reboots the watch into PRF, which invalidates the firmware slots,
then installs the firmware and the system resources; PRF takes both or
neither, so `--resources` is implied. PRF installs into slot 0, so on boards
with two firmware slots the build has to be the slot 0 one, the default.
Older app versions turn the Developer Connection off when the watch
reboots; over adb the runner starts it again, otherwise it asks for it to
be enabled again and waits.
A watch can also be updated by sideloading a {doc}`bundle`.

## Options

```{pbl-command} run
```
