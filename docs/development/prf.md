# PRF

PRF (Pebble Recovery Firmware) is a special firmware image available for recovery purposes.
It allows connecting from a phone to, for example, flash a new firmware image even if the main firmware is broken or unavailable.

## Building

Once a project is configured, PRF image can be built by running:

```shell
./waf build_prf
```

## Flashing

The PRF image can be flashed directly into the application area by running:

```shell
./waf flash_prf
```

This is useful when developing PRF features because the watch will boot directly to PRF.
To flash it to the external flash, so it can be used regularly, run:

```shell
./waf image_recovery
```

Append `--tty` option if needed.
In such case, PRF will need to be copied to the main flash to run.
This can be done by pressing {kbd}`BACK` + {kbd}`UP` + {kbd}`MIDDLE` when
booting, or holding {kbd}`BACK` for 7-10s while in the main application.

## Console

You can interact with PRF console by running:

```shell
./waf console_prf
```
