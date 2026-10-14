# Boot

The kernel owns the reset vector. `Reset_Handler()` sets the stack limit
registers on cores that have them and enters `kernel_prep_c()`, which copies
`.data` (and `.ramfunc` with `CONFIG_RAMFUNC`), zeroes
`.bss`, calls the SoC's `pbl_soc_early_init()` (`pbl/kernel/init.h`) for
vendor system init, clocks and caches, and then calls `main()`.
