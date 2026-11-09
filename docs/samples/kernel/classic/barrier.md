# Barrier

This sample builds a barrier from a mutex and a semaphore: three workers get
ready at their own pace, and none goes on until all have arrived.

The last one to arrive lets the others through and resets the count, so the
same barrier serves round after round.

```{pebbleos-sample}
:path: samples/kernel/classic/barrier
:sources: src/main.c
:output:
```
