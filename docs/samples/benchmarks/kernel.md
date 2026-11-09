# Kernel benchmark

This sample times the kernel's basic operations with the CPU cycle counter, a
thousand times each: uncontended semaphore, mutex and queue operations, an
allocation, switching to another thread and back, creating a thread, and
getting work to run on a queue.

On the native build a cycle is a nanosecond of
the host's clock and only says the sample runs; the ones worth comparing
between commits come from a watch. `empty measurement` is the cost of reading
the counter, included in every other line. The emulator's cycle counter does
not count, so the sample is not run there.

```{pebbleos-sample}
:path: samples/benchmarks/kernel
:sources: src/main.c
:output:
```
