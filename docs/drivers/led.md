# LED

An LED is set to a brightness and, on multicolor parts, a color. Consumers,
such as a backlight on an LED, keep the state and set it whole.

| | |
| --- | --- |
| Directory | `drivers/led` |
| Header | `pbl/drivers/led.h` |
| Kconfig | `CONFIG_LED`, selected by the drivers and code that use it |
| API | {apidoc}`API reference <drivers_led>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Awinic AW2016 | `awinic,aw2016` | `CONFIG_LED_AW2016` |
| GPIO LEDs | `gpio-leds` | `CONFIG_LED_GPIO` |
