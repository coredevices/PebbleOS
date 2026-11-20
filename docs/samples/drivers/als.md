# Ambient light: Lux readings

This sample reads the ambient light sensor five times, a second apart. It
primes the sensor first with `pbl_als_prime()`, which lets sensors that sample
continuously start before the first read, and releases it at the end. The
levels are raw counts up to `PBL_ALS_LEVEL_MAX`; `pbl_als_to_lux()` converts
them on boards that calibrate their sensor, and returns them unchanged
elsewhere.

On a watch, cover the sensor or point it at a light while the sample runs.
The emulator's simulated sensor reads 0.

```{pebbleos-sample}
:path: samples/drivers/als
:sources: src/main.c
:output:
```
