# Delayed work

This sample runs work after a delay, in three common shapes: a debounce, where
every edge of a button pushes the work back; a timeout, which keeps its first
delay; and a heartbeat that schedules itself again without drifting, until it
is canceled.

`pbl_delayed_work_cancel_sync()` returns with the heartbeat stopped for good,
although its function schedules it again: what a driver calls before freeing
the object the work lives in.

```{pebbleos-sample}
:path: samples/kernel/delayed_work
:sources: src/main.c
:output:
```
