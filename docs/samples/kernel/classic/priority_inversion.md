# Priority inversion

This sample has a low priority thread hold a mutex that `main()`, of high
priority, needs, while a medium priority thread that needs no lock keeps the
CPU busy. The mutex lends the high priority to its holder, so the wait lasts
as long as the low thread's work, not the medium one's.

Without inheritance, medium would run before low, and high would wait for a
thread it has nothing to do with. Semaphores do not inherit, as they have no
owner: another reason to guard data with mutexes.

```{pebbleos-sample}
:path: samples/kernel/classic/priority_inversion
:sources: src/main.c
:output:
```
