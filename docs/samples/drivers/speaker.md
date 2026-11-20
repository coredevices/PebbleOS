# Speaker: Streaming a tone

This sample plays a short melody on the board's speaker, the `SPEAKER`
audio device. It starts a stream over a ring of two periods, and the class
calls it back each time the hardware is done with one: the callback, which
runs in an interrupt, computes the next 20 ms of the sine and hands the
period back with `pbl_audio_period_ready()`. The ring comes from
`pbl_audio_buf_alloc()`, so the hardware can reach it, and is filled before
the stream starts.

On a watch, listen to four rising notes. The emulator plays them on the
host's audio output when run with `pbl run`; `pbl samples` runs it silent.

```{pebbleos-sample}
:path: samples/drivers/speaker
:sources: src/main.c
:output:
```
