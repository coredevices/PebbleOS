# Semaphores

This sample uses a semaphore in its two roles. As a signal, an interrupt (a
timer callback here) tells a waiting thread that a transfer is done, and the
thread gives up if it is not told in time. As a counter, it lets four workers
share two channels.

A give from an interrupt never blocks. The second transfer times out, and
stops the timer so a late interrupt cannot signal the next one.

```{pebbleos-sample}
:path: samples/kernel/semaphore
:sources: src/main.c
:output:
```
