# Core dumps

`subsys/coredump` saves the state of the system when it crashes: memory, and
the registers of every thread. The public API is in `pbl/coredump/`.

`pbl_coredump_trigger()` stops the system and writes the dump, then resets.
Firmware code calls it through `reset_with_core_dump()` in `pebble/system/die.h`,
and most crashes get there from `reset_due_to_software_failure()`. The
{doc}`../kernel/fatal` page describes the path from an assertion.

## What goes in a dump

Whoever owns some memory declares it, next to its own code:

```c
PBL_COREDUMP_REGION_DEFINE(ram, &__RAM_start__, (size_t)&__RAM_size__);
```

`PBL_COREDUMP_LATE_REGION_DEFINE()` is for memory whose read could hang or fault.
Those regions go after the threads, so the rest of the dump is saved by then,
and can have a function to make them readable. The LCPU RAM on the SF32LB52 is
one, in `soc/sf32lb/sf32lb52x/coredump.c`.

An image is a header followed by chunks, in the order regions, special
registers, threads, late regions. `pbl/coredump/format.h` documents the format,
and `tools/readcore.py` converts an image to an ELF core file:

```shell
tools/readcore.py coredump.bin coredump.elf
arm-none-eabi-gdb build/pebbleos.elf -ex "core-file coredump.elf"
```

## Backends

Every backend defined with `PBL_COREDUMP_BACKEND_DEFINE()` gets every dump. A
backend that fails stops getting the dump, without affecting the others, and the
core ends the image between chunks on one that is running out of room.
`pbl/coredump/backend.h` documents the interface. Backends must not use OS
services while a dump is being written.

`CONFIG_COREDUMP_BACKEND_FLASH` keeps dumps in flash, in the core dump partition
or, on boards without one, in the firmware update slot. A dump replaces an older
one, but not one less than a day old that has not been read, unless it is
forced. `CONFIG_COREDUMP_BACKEND_FLASH_IMAGES` sets how many dumps fit.

A backend that keeps dumps implements the `stored_*` operations, and
`pbl_coredump_stored_find()` and friends read them back. That is how the
get_bytes service sends a dump to a phone or to `pbl`.

## Porting

An architecture selects `ARCH_HAS_COREDUMP` and implements `pbl/coredump/arch.h`:
it captures its state, calls `pbl_coredump_arch_handler()`, and answers for the
registers of the threads that were running.

On Cortex-M the dump runs from the NMI, which can preempt anything, including a
fault handler. Fault handlers start with `PBL_ARCH_COREDUMP_FAULT_ENTRY`, which
saves what their C code would overwrite, so a thread that faulted appears in the
dump with its registers at the fault. Values that cannot be recovered read
`0xa5a5a5a5`: the frame of a fault that could not be stacked, and r4-r11 of a
thread interrupted by an ISR.

The platform implements `pbl_coredump_platform_prepare()`, which runs after the
backends' `init` and before their `start`, so the drivers they use already work
and nothing stored has been touched yet. It provides the serial number and build
ID to store. `pebble/kernel/core_dump_platform.c`
is the firmware's.
