# Touchscreen

A touchscreen reports touches and the gestures its controller recognizes to
the {doc}`input subsystem <../subsystems/input>`, mapped to display
coordinates from the devicetree. It is powered while it has users.

| | |
| --- | --- |
| Directory | `drivers/touch` |
| Header | `pbl/drivers/touch.h` |
| Kconfig | `CONFIG_TOUCH`, enabled by the application |
| Role | `TOUCH` |
| Sample | {doc}`../samples/drivers/touch` |
| API | {apidoc}`API reference <drivers_touch>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Hynitron CST816 | `hynitron,cst816s` | `CONFIG_TOUCH_CST816` |
| FocalTech FT6x36 | `focaltech,ft6236` | `CONFIG_TOUCH_FT6236` |
| QEMU Touchscreen | `qemu,pebble-touch` | `CONFIG_TOUCH_QEMU` |
| SDL mouse | — | `CONFIG_TOUCH_SDL` |
