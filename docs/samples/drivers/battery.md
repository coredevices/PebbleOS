# Battery

This sample reads the battery three times, a second apart: its voltage,
current and temperature, whether USB power is connected, whether the
charger charges and in which phase. It sets a connection handler too, which
prints USB power coming or going while it runs.

The battery is read through the charger of the board, its PMIC, which the
sample enables in its `boards/` files. Plug or unplug the charging cable
while it runs to see the handler and the charge state change.

```{pebbleos-sample}
:path: samples/drivers/battery
:sources: src/main.c
:output:
```
