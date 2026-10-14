# Boot

The kernel owns the reset vector. `Reset_Handler()` sets the stack limit
registers on cores that have them and enters `kernel_prep_c()`, which copies
`.data` (and `.ramfunc` with `CONFIG_RAMFUNC`), zeroes
`.bss`, calls the SoC's `pbl_soc_early_init()` (`pbl/kernel/init.h`) for
vendor system init, clocks and caches, and then calls `main()`.

```{mermaid}
flowchart LR
  reset["Reset_Handler()"]
  prep["kernel_prep_c()<br/>.data, .ramfunc, .bss"]
  soc["pbl_soc_early_init()<br/>system, clocks, caches"]
  main["main()<br/>fw/main.c"]
  reset --> prep --> soc --> main

  classDef app fill:#fa4a36,stroke:#fa4a36,color:#ffffff
  classDef system fill:#fd8a7c,stroke:#fd8a7c,color:#171717
  classDef base fill:#525252,stroke:#525252,color:#ffffff
  class reset,prep base
  class soc system
  class main app
```
