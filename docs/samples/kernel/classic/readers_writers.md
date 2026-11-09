# Readers and writers

This sample builds a lock that lets any number of threads read a record, or
one write it. The first reader in takes the room for all readers and the last
one out gives it back, which is why the room is a semaphore: a mutex is
unlocked by the thread that locked it.

This version favors readers: a writer waits as long as readers keep coming,
which the readers here avoid by staying out for a while after each read.

```{pebbleos-sample}
:path: samples/kernel/classic/readers_writers
:sources: src/main.c
:output:
```
