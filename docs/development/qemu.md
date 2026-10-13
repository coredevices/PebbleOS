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

```shell
pbl qemu
```

{doc}`../pbl/qemu` lists the ports the emulator exposes and its options.
With it running, other shells can attach to it:

- {doc}`../pbl/console` opens the firmware console.
- {doc}`../pbl/screenshot` captures the display.
- {doc}`../pbl/touch` and {doc}`../pbl/swipe` inject touch input on
  touch-capable boards.
- {doc}`../pbl/feed` writes phone data, such as weather, music,
  notifications and calendar events.
- {doc}`../pbl/debug` attaches GDB.

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
pbl qemu --bt-hci /dev/cu.usbmodemXXXX
```

`--bt-hci` takes any QEMU `-serial` spec, e.g. `tcp:localhost:9000`. On
macOS use the `cu.*` device: opening `tty.*` blocks until carrier detect.

The watch generates its identity address on first boot and keeps it in
flash, so pass `--keep-flash-image` to keep existing bondings across
launches. A fresh flash image gets a new address.

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
