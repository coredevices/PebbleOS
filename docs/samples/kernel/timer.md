# Timers

This sample starts a timer that repeats five times by starting itself again
from its callback, which wakes `main()` with a semaphore when done. It then
starts one, reads how long it has left, and stops it.

The callback runs in the tick interrupt, so it only counts and signals. Work
that has to block belongs in {doc}`delayed work <delayed_work>`, which runs on
a thread.

```{pebbleos-sample}
:path: samples/kernel/timer
:sources: src/main.c
:output:
```
