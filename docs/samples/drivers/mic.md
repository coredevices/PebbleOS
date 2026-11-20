# Microphone: Audio levels

This sample captures two seconds from the board's microphone, the `MIC`
audio device, and prints the peak and RMS level of every 200 ms. The class
calls the sample back each time the hardware has filled a period of the
ring; the callback, which runs in an interrupt, measures it, gives it back
with `pbl_audio_period_ready()` and queues the level of each 200 ms for the
main thread to print.

On a watch, speak or tap near the microphone while it listens: the levels
rise. The emulator's microphone is silent unless the host feeds it audio, so
it reports 0.

```{pebbleos-sample}
:path: samples/drivers/mic
:sources: src/main.c
:output:
```
