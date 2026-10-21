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

## Memory protection

`pbl/kernel/mpu.h` is the only interface to the MPU. A `struct pbl_mpu_region`
says what a range of memory is: who may access it, its memory type and whether
code can run from it. The kernel picks the slots and encodes the registers, so
nothing else knows either.

Privileged code can access anything no region covers through the default memory
map, and unprivileged code only what a region grants.

Static regions apply to every thread and last until reset:

- The kernel adds the firmware image, the read-only BSS and the ISR stack guard
  first thing on boot, before the SoC init.
- A SoC with `CONFIG_SOC_HAS_MPU_REGIONS` declares `pbl_soc_mpu_regions`, which
  the kernel adds next. Peripherals and memory shared with other cores go here.
- Anything else calls `pbl_mpu_region_add()` once the kernel runs, which is how
  the firmware adds the resource banks.

Static regions cannot overlap. The last four of the
`CONFIG_KERNEL_MPU_REGIONS` slots switch with the thread: up to two regions it
brings ({doc}`threads`) and its stack and syscall stack guards, which the kernel
adds.

`KERNEL_READONLY_DATA` places a variable in the read-only BSS, which apps can
read. ARMv8-M cannot let only privileged code write to a region, so all writes
go through `KERNEL_READONLY_DATA_SET()` or `pbl_mpu_user_ro_write()`, which lift
the region around the copy with interrupts masked.

`sys mpu` prints the programmed regions, as does a memory fault.

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

## External RAM

SoCs with external RAM select `CONFIG_EXTRAM`. Zero-initialized data marked
`PBL_SECTION_EXTRAM` lands there. It has no MPU region of its own: the
privileged background map covers it, so third-party code cannot reach it. On SF32LB52 boards, the SiP PSRAM is
enabled with `-DCONFIG_SF32LB52_PSRAM=y`.

Data marked `PBL_SECTION_DISPOSABLE` lands there too, but with
`CONFIG_PSRAM_RETENTION` it only keeps its contents while a retention hold is
taken with `psram_retain_acquire()` (`pbl/kernel/psram.h`). Holds never block
deep sleep. Deep sleep with a hold puts the PSRAM in half-sleep, which retains
the contents for about 30 uA; without one, in deep power-down, about 1 uA, and
disposable contents are lost. `psram_epoch()` then changes. Waking up leaves a
powered-down PSRAM alone, and the next hold pays for the exit.
`PBL_SECTION_EXTRAM` data is never lost: as long as there is any, the PSRAM is
never powered down.

Nothing disposable may assume it survives, and holds must follow what a user
does, not what is on screen: a screen redrawn every second would otherwise keep
the PSRAM retained for hours. Its users are:

- the volatile kernel heap (`kernel/volatile_heap.h`), 64 KiB for voice: its
  live allocations hold the PSRAM, and its headers are rebuilt after it was
  empty;
- the image slot pool (`kernel/psram_image_pool.h`), 32 slots of 44 KiB, and
  the image cache on top of it (`pbl/services/image_cache.h`). Images are
  received there, then copied to a resident image in internal RAM for as long
  as they are shown, so drawing never touches the PSRAM. The cache is kept
  for `CONFIG_PSRAM_IDLE_RELEASE_MS` after the last button press, backlight,
  track change or notification.

`sys psram` shows the part, the holds, the state and the epoch;
`sys volatile_heap` the heap usage, and `image cache` the cache entries.
