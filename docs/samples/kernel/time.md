# Time

This sample measures a sleep with the uptime, sets the wall clock and reads it
back a second later, and waits for a condition with `PBL_POLL_SLEEP_MS()`,
once for one that comes true and once for one that never does.

The uptime never jumps, so it is what durations and timeouts are measured
with; the wall clock is for telling the time.

```{pebbleos-sample}
:path: samples/kernel/time
:sources: src/main.c
:output:
```
