# Configuration Options

When configuring the build (`./pbl configure ...`) there are several options you can enable or tune.
Below you will find a list of the most relevant ones.

## Choosing your target

There are a number of target boards to choose from when building PebbleOS. You can do so by
using the (`--board`) flag followed by:

:`asterix`: (Core Devices) Pebble 2 Duo
:`obelix_dvt`, `obelix_pvt`: (Core Devices) Pebble Time 2
:`getafix_evt`, `getafix_dvt`, `getafix_dvt2`: (Core Devices) Pebble Round 2
:`qemu_emery`, `qemu_flint`, `qemu_gabbro`: dedicated QEMU targets (see {doc}`qemu`)

Keep in mind that some targets may not currently compile as-is.

## Variant

:`--variant`:
  Build variant, `normal` (main firmware) or `prf` (recovery firmware).

## Release build

:`-DCONFIG_RELEASE=y`:
  Build a release-mode firmware. Strips debug aids, enables shipping
  defaults (e.g. Memfault crash reporting), and reduces battery usage
  compared to a debug build. Pass this to `./waf configure`.

## Main features

:`-DCONFIG_MODDABLE_XS=y` / `-DCONFIG_MODDABLE_XS=n`:
  Force the Moddable SDK's XS JavaScript engine on or off, overriding
  the board defconfig. Pass to `./waf configure`. See {doc}`moddable`.
  PRF (recovery) builds always disable the engine regardless of this
  value.

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
