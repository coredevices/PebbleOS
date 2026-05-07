---
name: working-with-qemu
description: Use when building, launching, debugging, or capturing screenshots of PebbleOS under QEMU.
---

# Working with QEMU

PebbleOS can run under a custom QEMU version shipped with the PebbleOS SDK.

## Configure & build

```sh
./waf configure --board $BOARD
./waf build
```

where `$BOARD` is any of the `qemu_*` boards:

- `qemu_emery`
- `qemu_flint`
- `qemu_gabbro`

QEMU boards target a specific platform, e.g. `qemu_emery` targets the Emery platform, which is the platform used by Pebble Time 2.

## Launch

```sh
./waf qemu
```

The launched QEMU exposes:

- Interactive QEMU monitor on the launching terminal (`-monitor stdio`)
- Programmatic socket monitor (`-monitor unix:build/qemu-mon.sock`)
- Serial console over TCP on `localhost:12345` (console) and `localhost:12344` (pebble-tool)

UART1 output is also captured to `build/uart1.log`.

## Console

```sh
./waf console
```

Requires QEMU to be running.
Uses the TCP serial port to connect to the QEMU console, and provides a prompt for sending commands and receiving responses.

## Screenshot

```sh
./waf screenshot # defaults to build/screenshot.png
./waf screenshot --screenshot-output /tmp/foo.png
```

Requires QEMU to be running.
Uses the programmatic socket monitor to capture a screenshot of the QEMU display and save it to disk.
Useful to validate or iterate on UI changes.

## Interaction

Keyboard input is captured by QEMU, so you can interact with the PebbleOS UI.
Keys can also be send programmatically over the socket monitor using the `sendkey` command.
They key mapping is:

| QEMU key | PebbleOS key |
| -------- | ------------ |
| `left`   | `back`       |
| `right`  | `select`     |
| `up`     | `up`         |
| `down`   | `down`       |
