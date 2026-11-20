# Vibration: Pulses and braking

This sample buzzes the vibration motor three times. Then, if its driver can
brake, it buzzes three more times, each time driving the motor at the braking
strength from `pbl_vibe_get_braking_strength()` for a moment before stopping
it, which ends the buzz sooner.

On a watch, feel the difference between the two sets of pulses. The
emulator's simulated motor cannot brake.

```{pebbleos-sample}
:path: samples/drivers/vibe
:sources: src/main.c
:output:
```
