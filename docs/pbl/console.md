# `pbl console`

Attach to the firmware's console: over serial on a watch, over TCP to a
running emulator.

```shell
pbl console                       # the emulator
pbl console --tty /dev/ttyACM0    # a watch
```

Send `help` to list the console commands. Firmware log messages are hashed
at compile time, so the binary only contains a hash and the arguments; the
console dehashes them on the fly using the dictionary the build generates
at `build/fw/loghash_dict.json` (`build/prf/fw/loghash_dict.json` for PRF
builds). {doc}`../development/debugging/index` covers the flash log buffer and
coredumps.

## Options

```{pbl-command} console
```
