# Sensing

`subsys/sensing/` shares sensors between clients. Its public API is
`include/pbl/sensing/sensing.h`. A sensor of the
[sensor class](../drivers/sensor/index.md) has a single listener: the
sensing subsystem is that listener for every sensor it manages, and any
number of clients, each with its own type, rate and batch size, read the
sensor through it.

## Clients

A client is a `struct pbl_sensing_client`, owned by the code using it:

- `sensor` and `type`: what to read, e.g. `ACCEL` and `PBL_SENSOR_ACCEL`.
- `interval_us`: the sampling interval, or 0 for no frames (a client that
  only wants events).
- `batch`: how many frames, at `interval_us`, it accepts per update. 0 keeps
  the sensor sampling at `interval_us` but discards the frames.
- `events`: the events it wants, a bit per `enum pbl_sensor_event_type`.
- `notify` and `event`: its callbacks.

`pbl_sensing_client_add()` and `pbl_sensing_client_remove()` add and remove
it; `pbl_sensing_client_set_interval()`, `_set_batch()` and `_set_events()`
change it later.

## Arbitration

On every change the subsystem works out what the sensor must do:

- Each type samples at the shortest interval its clients ask for.
- Frames queue in the sensor for as long as the client with the shortest
  batch duration (`batch` times `interval_us`) allows, up to the depth of the
  sensor's FIFO. A driver whose batch size applies to the whole device gets
  the fastest type's last.
- An event is enabled while any client wants it, and goes to every client
  that wants it.

`pbl_sensing_suspend()` stops the sensor, keeping the clients, and restores
what they want when resumed.

## Data

Batches are stored once per sensor and type, as the driver produced them,
with the channel descriptors and timestamps of the batch. Consecutive batches
merge, so a sensor reporting every frame does not pay for a record per frame.
The buffer holds at least `CONFIG_SENSING_BUFFER_FRAMES` frames, or twice
the FIFO depth, and is allocated while a client reads the type.

Each client has its own position in the buffer. `pbl_sensing_read()`
decodes frames to the canonical unit, in the watch frame, and decimates them
to the client's interval with a phase accumulator: a 10 Hz client of a 25 Hz
sensor gets 2 frames in 5, evenly spread, and a 25 Hz client of a sensor
running at 26 Hz gets 25 frames in 26. A client that falls behind loses its
oldest frames, counted by `pbl_sensing_lost()`, and the others are not held
back.

A client calls `pbl_sensing_arm()` with a number of frames, and its
`notify` callback runs once a new batch brings that many. It is disarmed
during the call; it either reads the frames and arms again, or hands them
over and arms once they are consumed. Returning false keeps it armed, to try
again on the next batch.

`pbl_sensing_latest()` returns the newest frame without touching the
sensor, and `pbl_sensing_count()` counts the frames acquired.

## Context

Callbacks run on the high priority system work queue, from the sensor
listener, with the device lock held. All sensing functions take that lock,
so clients serialize their own state with it and may call back into the
subsystem from their callbacks, which must not block.

```c
static bool prv_notify(struct pbl_sensing_client *client) {
  int32_t ug[5][3];
  uint16_t n = pbl_sensing_read(client, &ug[0][0], 5U, nullptr);
  ...
  pbl_sensing_arm(client, 5U);
  return true;
}

static struct pbl_sensing_client s_client = {
  .sensor = ACCEL,
  .type = PBL_SENSOR_ACCEL,
  .interval_us = 40000,
  .batch = 5,
  .notify = prv_notify,
};

pbl_sensing_client_add(&s_client);
pbl_sensing_arm(&s_client, 5U);
```

## Users

- The accelerometer manager (`pebble/services/accel_manager/`) has a client
  per data subscriber, filling the subscriber's buffer and posting its
  callback to the subscriber's task, and one for shake and double tap
  events. The subscriber formats, the vibe flag and the sensitivity
  preference stay there.
- The e-compass (`pebble/services/ecompass/`) has a magnetometer client,
  whose rate rises during calibration, and an accelerometer client for tilt
  compensation.
