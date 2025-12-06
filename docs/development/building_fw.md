# Building firmware

Before building, make sure you've configured {doc}`./waf <../development/options>`. Then, run the following:

```shell
./waf build
```

## Loading firmware via Bluetooth

If you don't have a firmware development kit, you may bundle a `.pbz` file for sideloading 
onto your sealed watch:

```shell
./waf bundle
```

The resulting `.pbz` file will be located in the `build/` directory. Transfer this file
to the device paired to your watch, then, in the Pebble app, enable `Settings -> Show debug options`.
Go back to the Devices tab, tap your watch, then `Firmware Update Debug -> Sideload FW`, and select the `.pbz` file.


## Loading firmware with a firmware development kit

Before attempting to flash, check the documentation for each {doc}`board <../boards/index>` on how to prepare and connect your watch for programming.

You can flash the built firmware (including pre-compiled bootloader) by running:

```shell
./waf flash_fw --tty $SERIAL_ADAPTER
```
where `$SERIAL_ADAPTER` is the path for your serial adapter, e.g. `/dev/ttyACM0`, `/dev/tty.usbmodem1102`, etc.
If using a board with a built-in FTDI programmer, the `--tty` argument can be removed.

If flashing for the first time, your watch will reboot into PRF or a _sad watch_ state if PRF is missing, indicating that resources need to be flashed:

```shell
./waf image_resources --tty $SERIAL_ADAPTER
```

At this point you should observe the watch booting into the main application.
You can also see the logs by opening the console:

```shell
./waf console --tty $SERIAL_ADAPTER
```

Try sending `help` to get a list of available console commands.

