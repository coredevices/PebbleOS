# Magnetometer: Field readings

This sample has the magnetometer, `MAG`, measure the magnetic field five
times a second for 3 seconds, and prints the latest measurement every second
with `pbl_mag_peek()`. Before it starts sampling there is nothing to read:
the sensor sleeps until someone sets a sampling interval, and an interval of
0 puts it back to sleep. A listener counts the samples as they come.

On a watch, turn it around to see the field change. The emulator boards have
no magnetometer, so the sample uses the simulator there, chosen in its
`boards/<board>.roles.yml`, which measures no field.

```{pebbleos-sample}
:path: samples/drivers/mag
:sources: src/main.c
:output:
```
