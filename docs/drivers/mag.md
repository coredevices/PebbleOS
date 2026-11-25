# Magnetometer

A magnetometer reports each new sample to the listener a consumer registers,
the compass service in the Pebble firmware, or returns the latest one to
consumers that poll.

| | |
| --- | --- |
| Directory | `drivers/imu` |
| Header | `pbl/drivers/mag.h` |
| Kconfig | `CONFIG_MAG`, enabled by the application |
| Role | `MAG` |
| Sample | {doc}`../samples/drivers/mag` |
| API | {apidoc}`API reference <drivers_mag>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| MEMSIC MMC5603NJ | `memsic,mmc5603` | `CONFIG_MAG_MMC5603NJ` |
| Simulator | — | `CONFIG_MAG_SIM` |
