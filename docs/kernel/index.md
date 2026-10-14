# Kernel

`kernel/` owns every RTOS primitive the firmware uses. The public API lives in
`include/pbl/kernel/` and is the only threading interface the rest of the tree
may use. The implementation is PebbleOS's own.

## Topics

```{toctree}
:maxdepth: 1

threads.md
interrupts.md
boot.md
idle.md
memory.md
compiler.md
internals.md
```

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

## Configuration

`kernel/Kconfig` holds the tick rate, the number of priorities, the NVIC
priority levels the kernel uses, the thread limit, the TLS slot count and the
stack alignment the MPU guard needs.

`kernel/arch/arm/Kconfig` describes the CPU: each SoC selects
its core (`CPU_CORTEX_M4`, `CPU_CORTEX_M33`, `CPU_STAR_MC1`) and features
(`CPU_HAS_FPU`), the core selects its architecture (`ARMV7_M`,
`ARMV8_M_MAINLINE`) and the architecture its traits (`CPU_CORTEX_M_HAS_SPLIM`).
The compiler flags and the MPU backend are derived from these symbols. QEMU boards
pick the emulated core with `SOC_QEMU_CORTEX_M4` or `SOC_QEMU_CORTEX_M33`.

## Modules

The {apidoc}`API reference <kernel>` documents every kernel module.

## MCU and SoC

CPU core helpers (`pbl/mcu/`) wrap the architecture's interrupt, privilege,
cache, FPU and MPU control; `pbl/soc/` holds what is specific to one SoC
family.

The API reference documents both: {apidoc}`MCU <mcu>` and
{apidoc}`SoC <soc>`.

