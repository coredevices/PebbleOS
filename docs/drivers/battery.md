# Battery

A battery describes the cell a board has, from the devicetree, for chargers
and fuel gauges: its capacity, charge currents and voltage, internal
resistance and open-circuit voltage curve. It is a
description, not a device, built when the devicetree has one.

| | |
| --- | --- |
| Directory | `drivers/battery` |
| Header | `pbl/drivers/battery.h` |
| API | {apidoc}`API reference <drivers_battery>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Simple battery | `simple-battery` | `CONFIG_BATTERY_SIMPLE` |
