# Accelerometer

An accelerometer reports batches of samples, shakes and double taps to the
listener a consumer registers, the accelerometer service in the Pebble
firmware. Drivers fill buffers the sensor class owns and hide the hardware
state, such as FIFO modes: the consumer asks for a sampling interval and a batch size, and the
driver picks the configuration.

| | |
| --- | --- |
| Directory | `drivers/sensor` |
| Header | `pbl/drivers/sensor.h` |
| Kconfig | `CONFIG_ACCEL`, enabled by the application, selects `CONFIG_SENSOR` |
| Role | `ACCEL` |
| Sample | {doc}`../samples/drivers/accel` |
| API | {apidoc}`API reference <drivers_sensor>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| ST LIS2DW12 | `st,lis2dw12` | `CONFIG_ACCEL_LIS2DW12` |
| ST LSM6DSO | `st,lsm6dso` | `CONFIG_ACCEL_LSM6DSO` |
| Simulator | — | `CONFIG_ACCEL_SIM` |
