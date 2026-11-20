# Charger: Battery state

This sample reads the charger three times, a second apart: the battery
voltage, current and temperature, whether external power is present and the
charge status. It sets a listener too, which prints external power coming or
going while it runs.

Plug or unplug the charging cable while it runs to see the listener and the
charge status change.

```{pebbleos-sample}
:path: samples/drivers/charger
:sources: src/main.c
:output:
```
