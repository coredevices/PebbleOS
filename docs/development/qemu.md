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
pbl configure --board=$BOARD
pbl build
```

where `$BOARD` is one of the dedicated QEMU boards (`qemu_emery`,
`qemu_flint`, `qemu_gabbro`). Each targets a specific platform, e.g.
`qemu_emery` targets the Emery platform used by Pebble Time 2.

## Run

You can launch QEMU with the built image using:

```shell
pbl qemu
```

The flash image is rebuilt by default on every launch. To keep the existing flash image (e.g. to preserve stored apps), pass `--keep-flash-image`:

```shell
pbl qemu --keep-flash-image
```

The launched QEMU exposes:

- An interactive QEMU monitor on the launching terminal (`-monitor stdio`)
- A programmatic socket monitor (`-monitor unix:build/qemu-mon.sock`)
- The serial console over TCP on `localhost:12345` (console) and
  `localhost:12344` (pebble-tool)

UART1 output is also captured to `uart1.log` in the repository root.

## Console

You can launch a console using:

```shell
pbl console
```

It connects to the running QEMU over the TCP serial port and provides a
prompt for sending commands and receiving responses.

## Screenshots

With QEMU running, you can capture the display via the socket monitor:

```shell
pbl screenshot                                  # defaults to build/screenshot.png
pbl screenshot --output /tmp/foo.png
```

Useful to validate or iterate on UI changes.

## Interaction

Keyboard input is captured by the QEMU window, so you can interact with the
PebbleOS UI directly. Keys can also be sent programmatically over the socket
monitor using the `sendkey` command. The key mapping is:

| QEMU key | PebbleOS key |
| -------- | ------------ |
| `left`   | `back`       |
| `right`  | `select`     |
| `up`     | `up`         |
| `down`   | `down`       |

## Touch

On touch-capable boards you can inject touch events into a running QEMU.
Coordinates are given in screen pixels; the display size is read from the
emulated `pebble-touch` device and scaled automatically.

```shell
pbl touch 130 130                          # tap at (130, 130)
pbl swipe 130 220 130 40                   # swipe up (finger bottom -> top)
pbl swipe 130 220 130 40 --steps 20 --duration 0.4
```

Requires QEMU to be running; `pbl qemu` exposes the QMP socket used for
injection. A tap is a finger down then up; a swipe streams intermediate moves
so that drag gestures are seen as continuous. Injection uses the
absolute-pointer input path; multi-touch is not wired up in the device.

## Feeding phone data

Features that depend on the phone app can be exercised without one:
`pbl feed` writes into the running emulator what the phone would, over the
Pebble protocol serial port. Each kind of data is a subcommand:

```shell
pbl feed weather                        # a forecast for a few built-in cities
pbl feed weather Tokyo Sydney --seed 3   # a subset; the first is the current location
pbl feed weather --clear                 # remove every location
pbl feed music                           # a player and playlist; keeps serving until Ctrl-C
pbl feed music --title "Demo" --paused   # a single track of your own
```

`pbl feed --help` lists the feeds and `pbl feed <feed> --help` their options.
A feed that writes a database, like weather, exits once the data is stored.
One that stands in for the phone, like music, keeps running: it acts on the
watch's controls, advances playback and draws album art when asked (album
art is off by default, under Settings > Music). The emulator's port serves
one host client at a time, so run one such feed at once.
The feeds live in `tools/libs/pbl-cli/pbl/feeds/`, one module per kind of
data; see [the `pbl` CLI](pbl.md) for how to add one.

## Debug

You can debug with GDB using:

```shell
pbl debug
```

## Install .pbw applications

You can install applications coming from .pbw files onto QEMU. This requires [pebble-tool](https://github.com/coredevices/pebble-tool) to be installed and having `pebble` available in the $PATH.

Start by launching QEMU as you normally would:

```shell
pbl qemu
```

Inside another shell locate a .pbw file to install:

```shell
pebble install /path/to/your/file.pbw --qemu
```

The `pebble` CLI will detect the running QEMU instance and install the application. It should start automatically.
