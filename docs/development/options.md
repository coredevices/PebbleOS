# Configuration Options

When configuring the build (`./waf configure ...`) there are several options you can enable or tune.
Below you will find a list of the most relevant ones.

## Main features

:`--nojs`:
  Disable Javascript support

## Debugging

:`--nowatchdog`:
  Disable watchdog

:`--nostop`:
  Disable STOP mode (STM32 specific)

:`--nosleep`:
  Disable sleep mode (STM32 specific)

## Flashing

:`--jtag`:
  Choose alternative flash/debug probe.

## Logging

:`--log-level`:
  Default log level.

:`--nohash`:
  Disable log messages hashing.
  This will increase ROM usage, but will not require a dictionary file to decode logs.
