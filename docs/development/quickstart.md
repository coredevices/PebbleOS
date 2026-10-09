# Quickstart

This page takes you from a fresh machine to PebbleOS running in the emulator,
then to the same firmware installed on a watch. Each step gives the commands
and links to the page that explains them.

## 1. Install the toolchain

Install the PebbleOS SDK, which bundles the ARM toolchain, QEMU and the other
tools the build needs:

```shell
curl -LsSf https://github.com/coredevices/PebbleOS-SDK/releases/latest/download/pebbleos-sdk-installer.sh | sh
```

Then install the system packages for your OS. The lists for Ubuntu, Fedora
and macOS are in {doc}`getting_started`.

## 2. Get the source

Clone the repository with its submodules, enter it, and install the Python
dependencies in a virtual environment. This also installs `pbl`, the command
line the rest of this page uses:

```shell
git clone --recurse-submodules https://github.com/coredevices/pebbleos
cd pebbleos
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

```{tip}
Activate the virtual environment (`source .venv/bin/activate`) in every new
shell before running `pbl`.
```

See {doc}`getting_started` for the details and {doc}`pbl` for the tool.

## 3. Build for the emulator

Configure a build directory for one of the QEMU boards and build it.
`qemu_emery` targets the Emery platform (Pebble Time 2); `qemu_flint` and
`qemu_gabbro` target Pebble 2 Duo and Pebble Round 2:

```shell
pbl configure --board qemu_emery
pbl build
```

`pbl configure` runs CMake into `build/` and remembers the board, so later
commands do not need it again. Kconfig options are passed as
`-DCONFIG_...=y` on the configure line; see {doc}`options` and
{doc}`build_system`.

## 4. Run it in QEMU

```shell
pbl qemu
```

A window opens with the emulated watch and the firmware boots to the
watchface. The terminal that launched it is the QEMU monitor. The arrow keys
are the watch buttons: `left` is back, `right` is select, `up` and `down`
are up and down. Quit with `quit` at the monitor prompt or by closing the
window.

With the emulator running, other shells can attach to it:

```shell
pbl console       # the firmware's debug console
pbl screenshot    # writes build/screenshot.png
```

{doc}`qemu` covers the ports it exposes, Bluetooth, touch input and feeding
phone data into it.

## 5. Install an app on the emulator

With QEMU running and the [`pebble` tool](https://github.com/coredevices/pebble-tool)
on your `PATH`, install a `.pbw` from another shell:

```shell
pebble install /path/to/your/file.pbw --qemu
```

The app starts on the emulated watch. Pass `--keep-flash-image` to
`pbl qemu` on later launches to keep installed apps.

## 6. Build for a watch

Configure a second build directory for a real board. `asterix` is Pebble 2
Duo; the other boards and their revisions are listed in {doc}`options`:

```shell
pbl -b build-asterix configure --board asterix
pbl -b build-asterix build
pbl -b build-asterix bundle
```

`pbl bundle` writes a `.pbz` into the build directory, named
`normal_<board>_<version>.pbz`.

## 7. Install it on the watch

There are three ways to get the `.pbz` onto a watch:

1. **Sideload it in the Pebble mobile app.** Copy the `.pbz` to the phone
   paired with the watch. In the app, enable `Settings -> Show debug
   options`, then go back to the Devices tab, tap your watch, choose
   `Firmware Update Debug -> Sideload FW`, and select the file. The app
   starts the update and shows its progress on the watch card.
2. **Send it with the `pebble` tool.** With the
   [Developer Connection](https://developer.repebble.com/guides/tools-and-resources/developer-connection/)
   enabled in the Pebble mobile app, the
   [`pebble` tool](https://developer.repebble.com/sdk/) from the Pebble SDK
   installs the bundle over Bluetooth through the phone:

   ```shell
   pebble fw install build-asterix/normal_asterix_<version>.pbz --phone <phone-ip>
   ```

   Use `--cloudpebble` instead of `--phone` when the phone is connected
   through the CloudPebble proxy. `--slot 0|1` picks the slot for
   multi-slot bundles.
3. **Flash it with a firmware development kit.** With the kit connected
   over SWD or serial, `pbl -b build-asterix flash` writes the firmware
   directly, without the phone. This route, and the first-time resource
   flashing a dev kit needs, are in {doc}`building_fw`.

```{note}
Holding Back + Up + Select reboots the watch into the recovery firmware
(PRF), from which the Pebble mobile app can install a firmware again.
```
