# Emulator

The emulator runs PebbleOS on a desktop, without a watch. It is a fork of
[QEMU](https://www.qemu.org) that ships with the
[PebbleOS SDK](https://github.com/coredevices/PebbleOS-SDK), and it runs
the same firmware the watches do, built for an emulator board.

## What is emulated

The emulator does not model a particular watch's hardware. It emulates two
things:

The platform
: Each emulator board is a virtual machine for one SDK platform, with that
  platform's display, buttons and, where the platform has them, touch,
  backlight color, heart rate sensor and speaker. Apps see exactly what
  they would on a watch of that platform: the same screen size, color
  depth, shape and capabilities.

The architecture
: The machine has the same CPU as the watches of its platform, so the
  firmware is real ARM code, built with the same toolchain. Apps are ARM
  code too, so the `.pbw` files built with the app SDK, including those
  from the app store, install and run unmodified.

Everything below the platform is simplified. Instead of the nRF52840 or
SF32LB52 and the chips around it, the machine has a small set of devices
designed for the emulator, such as a framebuffer display, simple UARTs, an
RTC, a watchdog and an external flash, each with its own driver under
`drivers/*/qemu`. Hardware drivers, power management and radio timing are
therefore not exercised: use a watch for those.

| Board | Platform | Same platform as | CPU |
| --- | --- | --- | --- |
| `qemu_flint` | Flint | Pebble 2 Duo | Cortex-M4 |
| `qemu_emery` | Emery | Pebble Time 2 | Cortex-M33 |
| `qemu_gabbro` | Gabbro | Pebble Round 2 | Cortex-M33 |

The {doc}`native simulator <native_simulator>` is the other way to run the
firmware on a desktop: it is faster to build and easier to debug with host
tools, but it compiles for the host, so it cannot load apps.

## Building and running

Configure a build for an emulator board, build it and launch it:

```shell
pbl configure --board qemu_emery
pbl build
pbl run
```

A window opens with the watch's screen, drawn inside a bezel of the watch,
and the firmware boots. Each board offers a few bezels, in different
colors; `--decoration` picks another, or `none`. The terminal that launched it is the QEMU monitor;
quit with `quit` there or by closing the window.

The emulated watch keeps its data in two flash images in the build
directory: the internal flash, holding the firmware, and the external
flash, holding the resources, the file system and with it the installed
apps, settings and Bluetooth bondings. {doc}`../../pbl/run` rebuilds both on
every launch; pass `--keep-flash-image` to keep the external flash, and
with it the watch's data:

```shell
pbl run --keep-flash-image
```

The emulated clock follows the host's local time.

## Interacting

### Buttons

With the window focused, the arrow keys are the watch buttons:

| Key | Button |
| --- | --- |
| `left` | Back |
| `right` | Select |
| `up` | Up |
| `down` | Down |

The same keys can be sent from the monitor with `sendkey`, e.g.
`sendkey right`.

### Touch

On boards with touch, tap and swipe from a shell with
{doc}`../../pbl/emulator`:

```shell
pbl emulator touch 130 130
pbl emulator swipe 130 220 130 40
```

### Console and screenshots

Other shells attach to the running emulator:

```shell
pbl console               # the firmware's debug console
pbl emulator screenshot   # writes build/screenshot.png
```

### Phone data

There is no phone attached, but {doc}`pbl emulator feed <../../pbl/emulator>`
writes what the phone app would, such as weather, music, notifications and calendar events, over
the Pebble protocol port:

```shell
pbl emulator feed weather
```

### Apps

Install an app with the [`pebble` tool](https://github.com/coredevices/pebble-tool),
which finds the running emulator on its own:

```shell
pebble install /path/to/app.pbw --qemu
```

Launch with `--keep-flash-image` to keep installed apps across launches.

### Microphone

The emulator's microphone captures audio sent from the host, and silence
otherwise. With dictation or an app capturing, play a tone, noise, a WAV file
or your computer's microphone into it:

```shell
pbl emulator mic --file speech.wav
```

See {doc}`../../pbl/emulator` for the options. With the `pebble` tool
connected instead, use `pebble emu-mic --qemu localhost:12344`.

Apps need the phone to grant them the microphone, which only the SDK
emulator (`pebble install --emulator`) does; see
{doc}`../../reference/local-microphone`.

### Debugging

The emulator always runs a GDB server. {doc}`../../pbl/debug` attaches to it
through a proxy that walks the kernel's thread list, so `info threads` and
per-thread backtraces work; see {doc}`../debugging/index`.

## Ports and sockets

| Endpoint | Use |
| --- | --- |
| The launching terminal | Interactive QEMU monitor |
| `build/qemu-mon.sock` | Socket monitor, used by `pbl emulator screenshot` and scripts |
| `localhost:12345` | Serial console, used by `pbl console` |
| `localhost:12344` | Pebble protocol, used by the `pebble` tool and `pbl emulator feed` |
| `localhost:1234` | GDB server |
| `uart1.log` | The first UART's output, in the repository root |

## Bluetooth

By default the NimBLE host runs against a fake controller that accepts
every command but never forms a link. To use a real radio, attach an H4 HCI
controller to the emulator's fourth UART (QEMU pebble19 or later). An nRF52840 Dongle running
Zephyr's `hci_uart` sample shows up as a USB serial port and works out of
the box:

```shell
west build -b nrf52840dongle/nrf52840 zephyr/samples/bluetooth/hci_uart
nrfutil pkg generate --hw-version 52 --sd-req=0x00 \
  --application build/zephyr/zephyr.hex --application-version 1 hci_uart.zip
# press the dongle's reset button to enter the bootloader
nrfutil dfu usb-serial -pkg hci_uart.zip -p /dev/cu.usbmodemXXXX
```

Then build with the UART transport and pass the controller's port:

```shell
pbl configure --board qemu_flint -DCONFIG_BT_HCI_UART=y
pbl build
pbl run --bt-hci /dev/cu.usbmodemXXXX
```

`--bt-hci` takes any QEMU `-serial` spec, e.g. `tcp:localhost:9000`. On
macOS use the `cu.*` device: opening `tty.*` blocks until carrier detect.

The watch generates its identity address on first boot and keeps it in
flash, so pass `--keep-flash-image` to keep existing bondings across
launches. A fresh flash image gets a new address.


