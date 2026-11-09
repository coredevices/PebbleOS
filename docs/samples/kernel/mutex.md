# Mutexes

This sample has two threads add to a balance in two steps, a read and a write,
with the other thread getting in between. Without the mutex half of the
deposits are lost. It then waits for a mutex held elsewhere with a time limit,
and locks one twice.

Mutexes are recursive and inherit priority; {doc}`classic/priority_inversion`
shows why the latter matters. They cannot be used from interrupts, where a
{doc}`semaphore <semaphore>` is the tool.

```{pebbleos-sample}
:path: samples/kernel/mutex
:sources: src/main.c
:output:
```
