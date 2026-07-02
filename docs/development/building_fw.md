# Building firmware

Before building, make sure you've configured {doc}`./waf <../development/options>`. Then, run the following:

```shell
./waf build
```

## Loading firmware with a firmware development kit

Before attempting to flash, check the documentation for each {doc}`board <../boards/index>` on how to prepare and connect your watch for programming.

You can flash the built firmware by running:

```shell
./pbl flash
```

In some cases, you may have to specify the `--tty $SERIAL_ADAPTER` option where `$SERIAL_ADAPTER` is the path for your serial adapter, e.g. `/dev/ttyACM0`, `/dev/tty.usbmodem1102`, etc.

If flashing for the first time, you will also need to flash resources.
Some boards support direct resource programming by passing the `--resources` option.
The alternative is to flash while the firmware is running via the serial port using:

```shell
./pbl image_resources --tty $SERIAL_ADAPTER
```

When both firmware and resources are flashed, you should observe the watch booting into the main application.
You can also see the logs by opening the console:

```shell
./pbl console --tty $SERIAL_ADAPTER
```

Try sending `help` to get a list of available console commands.

## Loading firmware via Bluetooth

If you don't have a firmware development kit, you may bundle a `.pbz` file for sideloading
onto your sealed watch:

```shell
./waf bundle
```

The resulting `.pbz` file will be located in the `build/` directory. Transfer this file
to the device paired to your watch, then, in the Pebble app, enable `Settings -> Show debug options`.
Go back to the Devices tab, tap your watch, then `Firmware Update Debug -> Sideload FW`, and select the `.pbz` file.

## Logging (for firmware developer only)

Use `pulse_console.py` to view firmware logs via the Pulse protocol:

```shell
python tools/pulse_console.py --tty $SERIAL_ADAPTER
```

Where `$SERIAL_ADAPTER` is your serial port path (e.g. `COM3`, `/dev/ttyACM0`, `/dev/tty.usbmodem1102`).

The console supports interactive commands - type `help` to list available commands. Press `Ctrl-C` or `Ctrl-D` to exit.

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `PBL_CONSOLE_DICT_PATH` | Path to log hash dictionary | `build/src/fw/loghash_dict.json` |
| `PBL_CONSOLE_ARGS` | Comma-separated console options | - |

Available `PBL_CONSOLE_ARGS` options:
- `--justify=small|large` - Log line alignment
- `--color=yes|no` - Enable colored output
- `--bold=N` - Bold level (-1 for default)
- `--dict=PATH` - Override dictionary path
- `--core=yes|no` - Print core info

Example:
```shell
set PBL_CONSOLE_ARGS=--color=yes,--bold=0
python tools/pulse_console.py --tty COM3
```
