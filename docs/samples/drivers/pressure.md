# Pressure: Air pressure readings

This sample reads the air pressure five times, a second apart, through
`PRESSURE`: the board's pressure sensor, whatever drives it.
`pbl_pressure_read()` gives pascals; the sample prints hectopascals.

The reading is the pressure where the watch is, not corrected to sea level,
so it is lower than a weather report's by about 1.2 hPa for every 10 m of
altitude. Only boards with a pressure sensor run it; the emulator has none.

```{pebbleos-sample}
:path: samples/drivers/pressure
:sources: src/main.c
:output:
```
