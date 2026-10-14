# Idle

The SoC tickless-idle code talks to the kernel through `pbl/kernel/idle.h`:
`pbl_soc_idle()` and `pbl_soc_tick_enable()` are implemented per SoC, and
`pbl_idle_confirm()`, `pbl_idle_slept()` and `pbl_kernel_tick_isr()` are what
the kernel provides in return.
