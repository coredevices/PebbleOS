# Dining philosophers

This sample seats five threads at a table with five forks, mutexes, each
needing the two next to it. Taking the left one first deadlocks when everyone
holds one and waits for the next; taking the lower numbered one first cannot.

The deadlock is staged, as it otherwise takes bad luck: nobody goes on until
all hold a fork, and a time limit on the second fork stands in for waiting
forever. The fix is the rule the kernel and the drivers follow: locks are
always taken in the same order.

```{pebbleos-sample}
:path: samples/kernel/classic/dining_philosophers
:sources: src/main.c
:output:
```
