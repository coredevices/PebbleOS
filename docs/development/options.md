# Configuration Options

When configuring the build (`./waf configure ...`) there are several options you can enable or tune.
Below you will find a list of the most relevant ones.

## Choosing your target

There are a number of target boards to choose from when building PebbleOS. You can do so by
using the (`--board`) flag followed by:

:`snowy_bb2`: Pebble Time + Pebble Time Steel
:`spalding_bb2`: Pebble Time Round
:`silk_bb2`: Pebble 2 SE/HR
:`asterix`: (Core Devices) Pebble 2 Duo
:`obelix_dvt`, `obelix_pvt`: (Core Devices) Pebble Time 2

Keep in mind that some targets may not currently compile as-is.

## Main features

:`--js-engine`:
  Specify JavaScript engine (rocky or none). Default is 'rocky'.
  Use 'none' to disable JavaScript support.

## Debugging

:`--nowatchdog`:
  Disable watchdog

:`--nostop`:
  Disable STOP mode (STM32 specific)

:`--nosleep`:
  Disable sleep mode (STM32 specific)

## Flashing

:`--openocd-jtag`:
  Choose alternative flash/debug probe when using OpenOCD runner.

## Logging

:`--log-level`:
  Default log level.

:`--nohash`:
  Disable log messages hashing.
  This will increase ROM usage, but will not require a dictionary file to decode logs.
