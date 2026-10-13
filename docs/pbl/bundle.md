# `pbl bundle`

Package the firmware and its resources into a `.pbz` bundle, the format the
Pebble mobile app installs.

```shell
pbl bundle
```

The bundle is written to the build directory as
`<variant>_<board>_<version>.pbz`, with a `_slot<n>` suffix when the board
has more than one firmware slot.

## Sideloading with the Pebble app

Transfer the `.pbz` to the phone paired with the watch. In the Pebble app,
enable `Settings -> Show debug options`, go back to the Devices tab, tap
your watch, then `Firmware Update Debug -> Sideload FW`, and select the
file. The app starts the update and shows its progress on the watch card.

## Sideloading over adb

On Android, repeated installs are better scripted over adb, which installs
without asking. The broadcast needs an adb shell, which holds the
`android.permission.DUMP` it requires, and `Show debug options` on in the
app. `$PBZ` is the bundle:

```shell
adb push $PBZ /data/local/tmp/firmware.pbz
adb shell am broadcast -a coredevices.pebble.SIDELOAD_FIRMWARE \
    -n coredevices.coreapp/coredevices.pebble.firmware.FirmwareSideloadReceiver \
    --es path /data/local/tmp/firmware.pbz
```

The app waits up to a minute for the watch to be connected, then starts the
update and shows its progress on the watch card in the Devices tab.

Not every Android version lets an app read `/data/local/tmp`. If the app
reports the file as missing, create
`/sdcard/Android/data/coredevices.coreapp/cache` and push it there instead.

## Sideloading with the `pebble` tool

With the
[Developer Connection](https://developer.repebble.com/guides/tools-and-resources/developer-connection/)
enabled in the Pebble app, the [`pebble` tool](https://developer.repebble.com/sdk/)
installs the bundle over Bluetooth through the phone:

```shell
pebble fw install build/normal_asterix_<version>.pbz --phone <phone-ip>
```

Use `--cloudpebble` instead of `--phone` when the phone is connected
through the CloudPebble proxy. `--slot 0|1` picks the slot for multi-slot
bundles.

## Options

```{pbl-command} bundle
```
