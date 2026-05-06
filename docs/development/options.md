# Configuration Options

When configuring the build (`./waf configure ...`) there are several options you can enable or tune.
Below you will find a list of the most relevant ones.

## Choosing your target

There are a number of target boards to choose from when building PebbleOS. You can do so by
using the (`--board`) flag followed by:

:`asterix`: (Core Devices) Pebble 2 Duo
:`obelix_dvt`, `obelix_pvt`: (Core Devices) Pebble Time 2
:`getafix_evt`, `getafix_dvt`, `getafix_dvt2`: (Core Devices) Pebble Round 2
:`qemu_emery`, `qemu_flint`, `qemu_gabbro`: dedicated QEMU targets (see {doc}`qemu`)

Keep in mind that some targets may not currently compile as-is.

## Main features

:`--js-engine`:
  Specify JavaScript engine (moddable, or none). Default is 'moddable' with fallback to none if the board does not support Moddable XS.
  Use 'moddable' for the Moddable SDK's XS engine (see {doc}`moddable`).
  Use 'none' to disable JavaScript support.

## Debugging

:`--nowatchdog`:
  Disable watchdog

:`--nostop`:
  Disable STOP mode

:`--nosleep`:
  Disable sleep mode

## Flashing

:`--openocd-jtag`:
  Choose alternative flash/debug probe when using OpenOCD runner.

## Logging

:`--log-level`:
  Default log level.

:`--nohash`:
  Disable log messages hashing.
  This will increase ROM usage, but will not require a dictionary file to decode logs.
