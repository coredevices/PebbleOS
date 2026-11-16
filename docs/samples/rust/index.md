# Rust

Samples written in {doc}`Rust <../../development/build_system/rust>`, in
`samples/rust`. Each one does what its C counterpart does, on the `pebbleos`
crate.

```{toctree}
:maxdepth: 1

hello_world.md
```

```{toctree}
:maxdepth: 1
:caption: Kernel

kernel/threads.md
kernel/mutex.md
kernel/semaphore.md
kernel/msgq.md
kernel/poll.md
kernel/timer.md
kernel/work.md
kernel/delayed_work.md
kernel/heap.md
kernel/time.md
```

```{toctree}
:maxdepth: 1
:caption: Classic synchronization problems

kernel/classic/dining_philosophers.md
kernel/classic/bounded_buffer.md
kernel/classic/readers_writers.md
kernel/classic/sleeping_barber.md
kernel/classic/priority_inversion.md
kernel/classic/barrier.md
```
