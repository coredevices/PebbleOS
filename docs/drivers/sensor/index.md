# Sensors

A sensor (`include/pbl/drivers/sensor.h`) is a device measuring one or more
channel types: acceleration, angular rate, magnetic field, temperature or
pressure. A single device covers a part that measures several of them, as an
IMU with an accelerometer, a gyroscope and a temperature sensor sharing one
FIFO.

## Channels and units

Drivers do not convert their data. They describe each channel of a frame
with a `struct pbl_sensor_chan`: its type, its axis for vectors, its byte
offset and encoding, and the scale and bias taking the raw value to the
canonical unit of the type:

| Type | Values | Unit |
| --- | --- | --- |
| `PBL_SENSOR_ACCEL` | X, Y, Z | micro-g |
| `PBL_SENSOR_GYRO` | X, Y, Z | millidegrees per second |
| `PBL_SENSOR_MAGN` | X, Y, Z | nanotesla |
| `PBL_SENSOR_TEMP` | 1 | millidegrees Celsius |
| `PBL_SENSOR_PRESSURE` | 1 | millipascals |

The class decodes values on behalf of consumers with `pbl_sensor_read()`
and `pbl_sensor_decode()`. A driver whose conversion is not linear, as a
pressure sensor compensated with a polynomial of its calibration
coefficients, converts the value itself and stores it with a unit scale.
Descriptors are constant tables; a driver keeps one per configuration that
changes the scale, such as the full-scale range.

## Mounting and rotation

Vector channels are described in the sensor's own axes. The board gives the
sensor's orientation with the devicetree `mount-matrix` property, which the
driver passes to `PBL_SENSOR_INIT()` through `PBL_SENSOR_MOUNT_MATRIX()`.
The class applies it, and `pbl_sensor_set_rotated()` for watches worn
upside down, when it decodes vectors and when it reports events, so
consumers only see the watch frame and drivers never deal with either.

## Reading

`pbl_sensor_read()` returns the latest values of a type. The driver runs a
conversion when the sensor is idle, or returns the latest frame when it is
sampling. Its `read` operation fills a single frame, of up to
`PBL_SENSOR_FRAME_MAX` bytes, with its channel descriptors; the class decodes
it like any other frame.

## Streaming

A sensor has a single listener. Firmware services share sensors through the
[sensing subsystem](../../subsystems/sensing.md), which is that listener and
arbitrates the attributes below between its clients.

A consumer registers a listener with `pbl_sensor_set_listener()` and sets
the sampling interval and the batch size with `pbl_sensor_attr_set()`:

- `PBL_SENSOR_ATTR_INTERVAL` is the sampling interval in microseconds; the
  driver rounds it to what the hardware supports and writes it back.
- `PBL_SENSOR_ATTR_BATCH` is the largest number of frames the driver may
  queue before reporting them, which lets it sleep while a hardware FIFO
  fills. `PBL_SENSOR_ATTR_BATCH_MAX` is the depth of that FIFO.

A batch is a run of frames with the same layout, oldest first, with the time
of its last frame in microseconds of uptime and the interval between frames.
Hardware that interleaves channels sampled at different rates, such as tagged
FIFO entries, is reported as one batch per channel set.

Batches live in buffers owned by the class. The driver states how many it
needs and how large they are with `PBL_SENSOR_STREAM_STATE_DEFINE()`; the
buffers are aligned and padded for DMA, so a driver can read its FIFO
straight into one with `pbl_sensor_buf_alloc()` and hand it over without a
copy:

- `pbl_sensor_report()` calls the listener at once, from thread context with
  the device lock held.
- `pbl_sensor_submit()` is callable from any context, ISRs included: the
  class reports the batch from the high priority system work queue, in
  submission order. A driver can chain the FIFO read from its watermark
  interrupt with `pbl_i2c_transfer_async()` and submit the batch from the
  transfer callback, without waking a thread until there is data.

The buffer returns to the driver once the listener returns, unless the
listener keeps it with `pbl_sensor_hold()` to process it later and gives it
back with `pbl_sensor_release()`. While all buffers are taken the driver has
nowhere to read into; it leaves the data in its FIFO until one is free.

## Events

Events such as shakes and double taps are enabled with
`pbl_sensor_set_event()`. Their thresholds are attributes, as
`PBL_SENSOR_ATTR_SHAKE_SENSITIVITY`. The driver reports them with the axis in
its own frame, and the class remaps them to the watch frame.

## Roles

The board's sensors are reached through roles: `accel` as `ACCEL`, `mag` as
`MAG`, `die_temp` as `DIE_TEMP` and `pressure` as `PRESSURE`. The Kconfig menu
of each sensor type, `ACCEL`, `MAG`, `TEMP` and `PRESSURE`, groups its drivers
and requires its role.

## Shell

The `sensor` shell command, with `CONFIG_SENSOR_SHELL`, lists every sensor,
reads it and gets or sets its attributes:

```
> sensor list
0: tsen temp
> sensor read 0
temp: 31250 mC
```
