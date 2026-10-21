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
heap; `kernel/linker/memory.ld` how RAM is divided between the kernel, the
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

## Scratch heap

`pbl_scratch_malloc()` and `pbl_scratch_free()` (`pbl/kernel/scratch.h`) give
memory for short-lived data, apart from the kernel heap: buffers that only live
for the length of an operation. Its users do not know what memory backs it,
only that nothing should be kept there for long: on some boards that memory
draws power for as long as anything is allocated, so allocations must follow
what the user does, and be freed as soon as they are no longer needed.

By default the scratch heap is the kernel heap. On boards with RAM outside the
SoC's own, disabling `CONFIG_SCRATCH_HEAP_SHARED` puts it in that memory, up to
`CONFIG_SCRATCH_HEAP_SIZE`. In its own memory it is allocated first fit in
1 KiB units, with its maps in internal RAM: nothing of the heap is kept in the
memory itself, which it only uses while something is allocated.

### External RAM

A board describes its external RAM in its devicetree and gives it the `extram`
role. It is a device of the RAM class (`pbl/drivers/ram.h`, `CONFIG_RAM`),
which only powers it while it has users: the scratch heap takes it with its
first allocation and gives it back with its last free.
The first user powers the memory on; once the last one is gone, the next deep
sleep powers it off, so a burst of allocations does not power it up and down.
While it has users, deep sleep keeps its contents. It has no MPU region of its
own: the privileged background map covers it, so third-party code cannot
reach it.

The HyperRAM driver (`pbl/drivers/ram/hyperram.h`) knows the memory and not
its controller: it sizes it from its identification register, keeps it in
deep power-down with its controller gated when off, and in hybrid sleep in
deep sleep while used. It reaches it through its HyperBus controller
(`pbl/drivers/hyperbus.h`), the parent of its node.

`ram info` shows the power state and the scratch heap use, and `ram check`
writes and checks a buffer.
