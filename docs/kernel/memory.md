# Memory

## Code in RAM

SoCs that need code to run while flash is unavailable select
`CONFIG_RAMFUNC`, which adds a `.ramfunc` output section loaded from flash
and copied to RAM at boot. Code gets there in one of three ways:

- a function marked `PBL_SECTION_RAM`, or a constant marked
  `PBL_SECTION_RAM_RODATA` (`pbl/kernel/section.h`);
- whole source files, with `pbl_library_ramfunc(file.c ...)` next to the
  library's `pbl_library_sources()`;
- input section patterns registered against the `ramfunc` linker hook, for
  vendor code annotated its own way.

Without `CONFIG_RAMFUNC` all three are no-ops and the code stays in flash.

## Layout

The linker scripts document the memory map. `kernel/pebbleos.ld` explains
the sections, their load and run addresses, and the kernel's data, stack and
heap; `kernel/linker/memory.ld` how SRAM is divided between the kernel, the
app and the worker; and, on ARM, `kernel/arch/arm/linker/regions.ld` the
alignment the MPU imposes on those regions.

## Heaps

`pbl/kernel/heap.h` is a first-fit allocator over a region the caller
provides. A `struct pbl_heap` can be locked, can report double frees and
corruption to a handler instead of asserting, and, with
`CONFIG_MALLOC_INSTRUMENTATION`, records the caller of every block so
`sys heap` and `pbl heapstats` can attribute it.

The kernel owns one heap, between `_heap_start` and `_heap_end`.
`pbl/kernel/malloc.h` allocates from it: `pbl_malloc()`, `pbl_zalloc()`,
`pbl_calloc()`, `pbl_realloc()`, `pbl_strdup()` and `pbl_free()`, with
`_check` variants that never return NULL. They use the kernel heap whatever
thread calls them, and so do libc's `malloc()` and friends, which the
firmware links with `--wrap`.

Apps and workers have heaps of their own, in their RAM regions, which go
away with them. `applib/app_malloc.h` allocates from the app heap; applib
code that runs both for a process and for the kernel's own UI uses
`applib_malloc()`, which picks the heap of whoever it runs for. SDK apps'
`malloc()` resolves to it as well.

An allocation that cannot fail calls `pbl_heap_oom()`, which logs the heap
usage and calls `pbl_heap_oom_handler()`, implemented by the platform.
