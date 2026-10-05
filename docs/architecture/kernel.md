# Kernel

`kernel/` owns every RTOS primitive the firmware uses. The public API lives in
`include/pbl/kernel/` and is the only threading interface the rest of the tree
may use. The implementation is PebbleOS's own; its internals are described in
[kernel internals](kernel_internals.md).

## Layout

```
include/pbl/kernel/     public API: types, irq, thread, mutex, sem, msgq, poll, sched, idle, debug
include/pbl/kernel/backend.h   per-object private state the public structs embed
include/pbl/kernel/compiler.h  compiler abstraction, backed by compiler/gcc.h and compiler/clang.h
include/pbl/kernel/section.h   placement of code and data in special sections
kernel/                 scheduler, objects, tick conversion
kernel/arch/arm/        Cortex-M port: context switch, SVC, MPU, SysTick, idle, vector table
kernel/arch/posix/      host port for the unit tests
```

## Objects

| Object | Struct | Static define |
| --- | --- | --- |
| Thread | `struct pbl_thread` | `PBL_THREAD_STACK_DEFINE` + `pbl_thread_create` |
| Mutex | `struct pbl_mutex` | `PBL_MUTEX_DEFINE` |
| Semaphore | `struct pbl_sem` | `PBL_SEM_DEFINE` |
| Message queue | `struct pbl_msgq` | `PBL_MSGQ_DEFINE` |
| Poll group | `struct pbl_poll_group` | `PBL_POLL_GROUP_DEFINE` |

Conventions:

- Every object is a caller-owned struct; there is no create/destroy pair that
  hands out heap handles. A `PBL_*_DEFINE` is a complete static initialiser,
  so a defined object is usable before the scheduler starts and needs no init
  call. `pbl_*_init()` exists for objects that live in dynamically allocated
  memory.
- Blocking calls take a `pbl_timeout_t` (`PBL_NO_WAIT`, `PBL_FOREVER`,
  `PBL_MSEC()`, `PBL_TICKS()`). The struct type stops ticks/ms mix-ups.
- Return `int`: 0 on success, `-EAGAIN` on timeout, `-EBUSY` for `PBL_NO_WAIT`.
- No `_from_isr` variants: `pbl_in_isr()` picks the path and any needed
  context switch is requested inside the call.
- Mutexes are recursive with owner and lock-site tracking. Non-recursive
  intent is expressed with `pbl_mutex_assert_held(m, false)`.
- Priorities: higher is more urgent, `PBL_PRIO_IDLE` .. `PBL_PRIO_MAX`.
- Returning from a thread entry function ends the thread.

### Threads

`pbl_thread_create()` takes a `struct pbl_thread_attr`: entry, priority,
privileged flag, a caller-owned stack and up to four `MpuRegion`s that are
switched in with the thread. `pbl_thread.tls[]` holds the per-thread pointers
the syscall layer needs.

### Introspection

`debug.h` covers what core dumps, fault handling, stack checks and telemetry
need: a thread walk with saved registers in the canonical order the core dump
format expects, saved PC/LR/CONTROL of a blocked thread, stack bounds and
high-water marks, and a run-time stats snapshot.

### Interrupts

SoC interrupts are bound at build time; there is no runtime handler
registration and the vector table is in flash:

```c
PBL_IRQ_CONNECT(I2C1, 5, i2c_irq_handler, I2C1_BUS, 0);

PBL_IRQ_DIRECT(AON, 0, PBL_IRQ_ZERO_LATENCY) {
  /* handler body */
}
```

A line is named after its `PBL_SOC_IRQN_<line>` define in the SoC's
checked-in `soc_irqs.h`, which also gives its number: `PBL_IRQN(I2C1)` is
`PBL_SOC_IRQN_I2C1`. The defines were generated once from the vendor
`IRQn_Type`, so the names match the CMSIS `<line>_IRQn`, and every binding
asserts that the two numbers still agree. The SoC's line count is
`CONFIG_NUM_IRQS`, set in `soc/*/Kconfig.defconfig`.

`PBL_IRQ_CONNECT()` calls `isr(arg)` with whatever type `isr` takes (an
empty `arg` calls `isr()`); `PBL_IRQ_DIRECT()` takes the handler body
instead. Binding a line twice fails to link, and a line without a
`PBL_SOC_IRQN_<line>` fails to compile.

The priority is in controller units (0 is the most urgent) and is
programmed for every connected line by `pbl_irq_init()` at boot, so drivers
only call `pbl_irq_enable()` and `pbl_irq_disable()`. A priority more urgent
than `PBL_IRQ_PRIO_MAX_SYSCALL` is a build error unless the line is flagged
`PBL_IRQ_ZERO_LATENCY`, in which case the ISR must not call the kernel. An
enabled line nobody connected lands in `arch_irq_spurious()`, which asserts.

### Boot

The kernel owns the reset vector. `Reset_Handler()` sets the stack limit
registers on cores that have them and enters `kernel_prep_c()`, which copies
`.data` (and `.ramfunc` with `CONFIG_RAMFUNC`), zeroes
`.bss`, calls the SoC's `pbl_soc_early_init()` (`pbl/kernel/init.h`) for
vendor system init, clocks and caches, and then calls `main()`.

### Idle

The SoC tickless-idle code talks to the kernel through `pbl/kernel/idle.h`:
`pbl_soc_idle()` and `pbl_soc_tick_enable()` are implemented per SoC, and
`pbl_idle_confirm()`, `pbl_idle_slept()` and `pbl_kernel_tick_isr()` are what
the kernel provides in return.

### Code in RAM

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

## Compiler abstraction

`pbl/kernel/compiler.h` is the only place the tree may spell compiler
specifics: attributes (`PBL_PACKED`, `PBL_WEAK`, `PBL_NORETURN`,
`PBL_SECTION()`, ...) and builtins (`PBL_LIKELY()`, `PBL_UNREACHABLE()`,
`PBL_CLZ()`, ...). Every public macro is declared and documented once in the
frontend and expands to a `*_IMPL` counterpart from the backend selected by
the predefined macros: `compiler/gcc.h` for GCC and `compiler/clang.h` for
Clang, which reuses the GCC definitions and blanks the attributes Clang does
not implement. Supporting another compiler means adding a backend that
defines the same `*_IMPL` set and a branch in the frontend; callers stay
untouched. The unit-test hooks built on these (`PBL_T_STATIC`,
`PBL_T_MOCKABLE`) live in `pbl/util/testing.h`. Code outside
`include/pbl/kernel/compiler/` must not use `__attribute__` or `__builtin_*`
directly; the header is shipped with the SDK so exported headers follow the
same rule.

## Configuration

`kernel/Kconfig` holds the tick rate, the number of priorities, the NVIC
priority levels the kernel uses, the thread limit, the TLS slot count and the
stack alignment the MPU guard needs.

`kernel/arch/arm/Kconfig` describes the CPU, as in Zephyr: each SoC selects
its core (`CPU_CORTEX_M4`, `CPU_CORTEX_M33`, `CPU_STAR_MC1`) and features
(`CPU_HAS_FPU`), the core selects its architecture (`ARMV7_M`,
`ARMV8_M_MAINLINE`) and the architecture its traits (`CPU_CORTEX_M_HAS_SPLIM`).
The compiler flags and the MPU backend are derived from these symbols. QEMU boards
pick the emulated core with `SOC_QEMU_CORTEX_M4` or `SOC_QEMU_CORTEX_M33`.
