# Idle

When nothing is runnable, the idle thread asks the kernel to idle the CPU
until the next timeout. The kernel owns everything about time in that
decision; the SoC only describes how its CPU can sleep, through
`pbl/kernel/idle.h`:

- `pbl_soc_idle_states[]` lists the idle states, shallowest first. Each has
  the shortest time until the next timeout worth entering it for, the time it
  takes to wake up from, and an `enter()` function that sleeps until an
  interrupt is pending.
- `pbl_soc_idle_state_limit()` returns the deepest state allowed right now,
  or -1 to keep the CPU running, from whatever the SoC tracks: peripherals
  that cannot run in deep sleep, an IPC queue still busy, a debug override.

The kernel picks the deepest allowed state worth entering, programs the
system clock event device to fire that state's wake-up time ahead of the
next timeout, enters it with every interrupt masked, then catches the tick
count up from the system clock source and goes back to a tick at a time.
Interrupts that woke the CPU run once the tick is back.

The time spent in each state is kept per state
(`pbl_idle_residency_us()`); `sys cpustats` prints it and the analytics
report it as `cpu_sleepN_pct`, N being the state index.
