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
7. calls `main()`.

```{mermaid}
flowchart LR
  reset["Reset_Handler()"]
  prep["kernel_prep_c()<br/>.data, .ramfunc, .bss"]
  init["kernel_init()<br/>SoC, IRQs, faults, heap,<br/>console, banner"]
  main["main()<br/>fw/main.c"]
  reset --> prep --> init --> main

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class reset,prep base
  class init system
  class main app
```
