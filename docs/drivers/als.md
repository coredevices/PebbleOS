# Ambient light sensor

An ambient light sensor reads raw counts, which a calibrated instance
converts to lux. Consumers prime it before they read, and code that would
disturb a reading, such as the backlight, suspends it.

| | |
| --- | --- |
| Directory | `drivers/als` |
| Header | `pbl/drivers/als.h` |
| Kconfig | `CONFIG_ALS`, enabled by the application |
| Role | `ALS` |
| Sample | {doc}`../samples/drivers/als` |
| API | {apidoc}`API reference <drivers_als>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| TI OPT3001 | `ti,opt3001` | `CONFIG_ALS_OPT3001` |
| Sensortek W1160 | `sensortek,w1160` | `CONFIG_ALS_W1160` |
| Simulator | — | `CONFIG_ALS_SIM` |
