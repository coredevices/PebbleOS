# Magnetometer

A magnetometer reports batches of samples to the listener a consumer
registers, the compass service in the Pebble firmware, or runs a single
measurement for consumers that read it.

| | |
| --- | --- |
| Directory | `drivers/sensor` |
| Header | `pbl/drivers/sensor.h` |
| Kconfig | `CONFIG_MAG`, enabled by the application, selects `CONFIG_SENSOR` |
| Role | `MAG` |
| Sample | {doc}`../samples/drivers/mag` |
| API | {apidoc}`API reference <drivers_sensor>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| MEMSIC MMC5603NJ | `memsic,mmc5603` | `CONFIG_MAG_MMC5603NJ` |
| Simulator | — | `CONFIG_MAG_SIM` |
