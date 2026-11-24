# Backlight

A backlight sets the brightness, and on multicolor parts the color, of the
light behind a display.

| | |
| --- | --- |
| Directory | `drivers/backlight` |
| Header | `pbl/drivers/backlight.h` |
| Kconfig | `CONFIG_BACKLIGHT`, enabled by the application |
| Role | `BACKLIGHT` |
| Sample | {doc}`../samples/drivers/backlight` |
| API | {apidoc}`API reference <drivers_backlight>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| AW9364E | `awinic,aw9364e` | `CONFIG_BACKLIGHT_AW9364E` |
| LED | `led-backlight` | `CONFIG_BACKLIGHT_LED` |
| PWM | `pwm-backlight` | `CONFIG_BACKLIGHT_PWM` |
| QEMU | `qemu,pebble-display` | `CONFIG_BACKLIGHT_QEMU` |
| POSIX | — | `CONFIG_BACKLIGHT_POSIX` |
