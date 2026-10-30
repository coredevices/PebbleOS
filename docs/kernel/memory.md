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

## External RAM

SoCs with external RAM select `CONFIG_EXTRAM`. Zero-initialized data marked
`PBL_SECTION_EXTRAM` lands there, in a privileged, non-executable MPU region
that third-party code cannot reach. On SF32LB52 boards, the SiP PSRAM is
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
