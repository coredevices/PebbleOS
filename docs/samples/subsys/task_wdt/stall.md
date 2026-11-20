# Task watchdog: Stalled thread

This sample watches its main thread with the
{doc}`task watchdog <../../../subsystems/task_watchdog>`, on a channel that
expires after a second without a feed.

Work that feeds the channel in time is never reported, and neither is
waiting for work once the thread marks itself as waiting. Then the thread
gets stuck on a semaphore nobody gives: the watchdog logs the stuck thread
and calls the channel callback, which frees it. The sample overrides the
`pbl_task_wdt_stalled()` and `pbl_task_wdt_recovered()` hooks to print what
happens; the system uses them for its own reports.

On boards with a hardware watchdog, a channel that stayed expired for the
grace period would reset the system. Here the callback recovers the thread
first.

```{pebbleos-sample}
:path: samples/subsys/task_wdt/stall
:sources: src/main.c
:output:
```
