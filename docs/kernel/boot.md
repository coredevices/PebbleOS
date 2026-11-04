# Boot

The kernel owns the reset vector. `Reset_Handler()` sets the stack limit
registers on cores that have them and enters `kernel_prep_c()`, which copies
`.data` (and `.ramfunc` with `CONFIG_RAMFUNC`) and zeroes `.bss`. On the
host, the posix boot thread takes its place.

Both continue in `kernel_init()`, which:

1. calls the SoC's `pbl_soc_early_init()` (`pbl/kernel/init.h`) for vendor
   system init, clocks and caches;
2. programs the priority of every bound interrupt with `pbl_irq_init()`;
3. runs the arch's early setup, enabling the fault exceptions on ARM;
4. initializes the kernel heap;
5. lets each log backend bring up its output (`pbl_log_early_init()`): the
   console backend sets the board's console port, `CONSOLE_UART`, up for
   polled transmission;
6. prints the boot banner, with the version from `pbl/version.h`. Backends
   that start later, like the flash one, get it replayed from the logging
   boot buffer;
7. creates the main thread, whose stack and priority come from
   `CONFIG_KERNEL_MAIN_STACK_SIZE` and `CONFIG_KERNEL_MAIN_THREAD_PRIO`, and
   starts the scheduler, which runs `main()` on it.

```{mermaid}
flowchart LR
  reset["Reset_Handler()"]
  prep["kernel_prep_c()<br/>.data, .ramfunc, .bss"]
  init["kernel_init()<br/>SoC, IRQs, faults, heap,<br/>console, banner"]
  main["main()<br/>main thread"]
  reset --> prep --> init --> main

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class reset,prep base
  class init system
  class main app
```

With `CONFIG_KERNEL_STACK_GUARD`, a thread or the ISR overflowing its stack
faults instead of corrupting the memory below it. Cores with stack limit
registers use those; others get a no-access MPU region below every stack, in
room `PBL_THREAD_STACK_DEFINE` adds on top of the size it is given. Stack sizes
always mean usable space.
