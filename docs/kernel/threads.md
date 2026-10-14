# Threads

`pbl_thread_create()` takes a `struct pbl_thread_attr`: entry, priority,
privileged flag, a caller-owned stack and up to four `MpuRegion`s that are
switched in with the thread. `pbl_thread.tls[]` holds the per-thread pointers
the syscall layer needs.

## Introspection

`debug.h` covers what core dumps, fault handling, stack checks and telemetry
need: a thread walk with saved registers in the canonical order the core dump
format expects, saved PC/LR/CONTROL of a blocked thread, stack bounds and
high-water marks, and a run-time stats snapshot.
