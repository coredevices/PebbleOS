# Threads

This sample creates three {doc}`threads <../../kernel/threads>` from `main()`:
one of higher priority, which runs to its end before `pbl_thread_create()`
returns, and two of lower priority, which only run once `main()` blocks and
then take turns by yielding.

`ping` and `pong` have the same priority, so the kernel also switches between
them on every tick: their lines usually alternate, but only the totals are
guaranteed.

```{pebbleos-sample}
:path: samples/kernel/threads
:sources: src/main.c
:output:
```
