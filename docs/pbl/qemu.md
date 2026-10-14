# `pbl qemu`

Launch the firmware of a QEMU board build in the emulator.

```shell
pbl configure --board qemu_emery
pbl build
pbl qemu
```

The flash image is rebuilt on every launch; `--keep-flash-image` keeps it,
and with it installed apps, data and Bluetooth bondings.

The launched QEMU exposes:

- An interactive QEMU monitor on the launching terminal (`-monitor stdio`)
- A programmatic socket monitor (`-monitor unix:build/qemu-mon.sock`)
- The serial console over TCP on `localhost:12345`, for {doc}`console`
- The Pebble protocol over TCP on `localhost:12344`, for the `pebble` tool
  and {doc}`feed`

UART1 output is also captured to `uart1.log` in the repository root.
{doc}`../development/emulation/emulator` covers what the emulator models and how to
operate it.

## Options

```{pbl-command} qemu
```
