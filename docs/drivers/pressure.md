# Pressure sensor

A pressure sensor runs one conversion per read, and sleeps in between.

| | |
| --- | --- |
| Directory | `drivers/sensor` |
| Header | `pbl/drivers/sensor.h` |
| Kconfig | `CONFIG_PRESSURE`, enabled by the application, selects `CONFIG_SENSOR` |
| Role | `PRESSURE` |
| Sample | {doc}`../samples/drivers/pressure` |
| API | {apidoc}`API reference <drivers_sensor>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Bosch BMP380 and BMP390 | `bosch,bmp380` | `CONFIG_PRESSURE_BMP390` |
