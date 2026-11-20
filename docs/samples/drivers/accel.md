# Accelerometer: Samples and gestures

This sample reads the accelerometer, `ACCEL`, once with
`pbl_accel_peek()`, then has it sample every 40 ms for 5 seconds, in
batches of up to 10 samples, and counts the shakes and double taps it
detects. A listener set with `pbl_accel_set_listener()` gets the batches and
events. Batches carry the sensor's raw samples with how to scale them to
milli-g, and the listener runs with the device lock held, so the main thread
takes that lock to read what it wrote.

On a watch, move it, shake it or tap it twice while the sample runs. On the
emulator the simulator reports the watch lying flat, `z=-1000`, and no
events.

```{pebbleos-sample}
:path: samples/drivers/accel
:sources: src/main.c
:output:
```
