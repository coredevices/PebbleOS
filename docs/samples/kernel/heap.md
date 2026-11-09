# Heaps

This sample gives a 2 KiB buffer a heap of its own, with a mutex as its lock,
and watches its usage: full, then with every other block freed, where plenty
of memory is free but no block of twice the size fits, then empty again.

Sizes include a header per block, and differ between a watch and the native
build. For memory that needs no bound of its own, `pbl_malloc()` allocates
from the kernel heap.

```{pebbleos-sample}
:path: samples/kernel/heap
:sources: src/main.c
:output:
```
