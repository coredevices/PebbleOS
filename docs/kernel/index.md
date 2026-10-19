# Kernel

`kernel/` owns every RTOS primitive the firmware uses. The public API lives in
`include/pbl/kernel/` and is the only threading interface the rest of the tree
may use. The implementation is PebbleOS's own.

## Topics

```{toctree}
:maxdepth: 1

threads.md
syscalls.md
interrupts.md
time.md
boot.md
idle.md
memory.md
fatal.md
compiler.md
internals.md
```

## Layout

```{mermaid}
block-beta
  columns 2
  api["Public API · include/pbl/kernel/<br/>threads, mutexes, semaphores, queues, poll, IRQs"]:2
  core["Kernel · kernel/<br/>scheduler, objects, ticks"]:2
  arm["Cortex-M port · kernel/arch/arm/<br/>context switch, SVC, MPU, SysTick"] posix["Host port · kernel/arch/posix/<br/>native simulator, unit tests"]

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class api app
  class core system
  class arm,posix base
```

Next to the API, `include/pbl/kernel/` holds:

- `backend.h`: the private state each public object struct embeds;
- `compiler.h`: the {doc}`compiler abstraction <compiler>`;
- `section.h`: the placement of code and data in special sections.

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

CPU core helpers (`pbl/mcu/`) wrap the architecture's interrupt, cache, FPU
and MPU control; `pbl/soc/` holds what is specific to one SoC
family.

The API reference documents both: {apidoc}`MCU <mcu>` and
{apidoc}`SoC <soc>`.

