# Pressure sensor

A pressure sensor runs one conversion per read, and sleeps in between.

| | |
| --- | --- |
| Directory | `drivers/pressure` |
| Header | `pbl/drivers/pressure.h` |
| Kconfig | `CONFIG_PRESSURE`, enabled by the application |
| Role | `PRESSURE` |
| Sample | {doc}`../samples/drivers/pressure` |
| API | {apidoc}`API reference <drivers_pressure>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Bosch BMP380 and BMP390 | `bosch,bmp380` | `CONFIG_PRESSURE_BMP390` |
