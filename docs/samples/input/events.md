# Input events

This sample logs every event the input devices report for 30 seconds: keys
pressed and released, touch axes and gestures. It registers a callback with
`PBL_INPUT_CALLBACK_DEFINE()`, which drivers may call from an interrupt, so
the callback only queues the event and the main thread logs it.

Run it on a board and press its keys or touch its screen while it listens;
on the emulator nothing comes, and it reports 0 events.

```{pebbleos-sample}
:path: samples/input/events
:sources: src/main.c
:output:
```
