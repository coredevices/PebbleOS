# Temperature: Die readings

This sample reads the temperature of the chip five times, a second apart,
through `DIE_TEMP`: the board's die temperature sensor, whatever drives it.
`pbl_temp_read()` gives millidegrees Celsius.

On a watch the reading follows the chip, a few degrees above the room and
rising under load. The emulator's simulated sensor reads a fixed 25 C.

```{pebbleos-sample}
:path: samples/drivers/temp
:sources: src/main.c
:output:
```
