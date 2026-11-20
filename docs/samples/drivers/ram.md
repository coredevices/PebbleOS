# RAM: Pattern test

This sample uses the board's external RAM, the `EXTRAM` device: it becomes
a user of it with `pbl_ram_get()`, which powers it on, writes a pattern to
its first MiB through the CPU mapping, reads it back, and lets it go with
`pbl_ram_put()`. Without users, the next deep sleep powers the memory off
and its contents are lost.

With `CONFIG_SCRATCH_HEAP_SHARED` off, the kernel scratch heap takes this
memory, and code allocates from it instead; the sample leaves that option
on, so the memory is its own to write.

```{pebbleos-sample}
:path: samples/drivers/ram
:sources: src/main.c
:output:
```
