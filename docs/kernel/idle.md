# Idle

When nothing is runnable, the idle thread asks the kernel to idle the CPU
until the next timeout.

Idle states are optional. Without `CONFIG_CPU_IDLE` the kernel waits for an
interrupt (`pbl_cpu_idle()`) a tick at a time: the tick keeps running, the
SoC provides nothing and none of what follows is built. That is enough
where power does not matter, e.g. a bootloader.

With `CONFIG_CPU_IDLE` the kernel owns everything about time in the
decision; the SoC only describes how its CPU can sleep, through
`pbl/kernel/idle.h`. `pbl_soc_idle_states[]` lists the idle states,
shallowest first. A SoC that only waits for an interrupt provides nothing:
without `CONFIG_SOC_HAS_IDLE_STATES` the kernel defines that single state. Each has the shortest time until the next timeout worth
entering it for, the time it takes to wake up from, and an `enter()`
function that sleeps until an interrupt is pending. `pbl_cpu_idle()` is the
plain wait for interrupt, for the shallowest state and for deeper ones to
wrap, and `pbl_cpu_idle_deep()` the same in the CPU's deep sleep. The times
of the states the hardware defines come from the devicetree; see
{ref}`devicetree-idle-states`.

A driver whose peripheral does not work in a state keeps the CPU out of it,
and of every deeper one, while busy, with `pbl_idle_state_block()` and
`pbl_idle_state_release()`. The SoC names its states after the datasheet for
those calls. They do nothing without `CONFIG_CPU_IDLE`, so drivers call them
unconditionally.

Code that must run within some time of an interrupt, whatever the state,
asks for it instead, with `pbl_idle_latency_request_add()`: the kernel then
skips the states and hooks that take longer to wake up from, and the
shortest request applies.

A driver that has something to do while the CPU idles, e.g. power its
peripheral down, defines a hook with `PBL_IDLE_HOOK_DEFINE()`: a function
called before the CPU enters a state and another after it left it, both told
which state. A hook has its own costs, like a state: the shortest time until
the next timeout worth calling it for, and the time its exit function takes.
They often depend on the part behind the driver, so the driver sets them
once it knows them, with `pbl_idle_hook_set_times()`; the hook is not called
before. The kernel calls the hooks that are worth it around whichever state it
enters, and wakes the CPU up that much earlier for them. Hooks run with
every interrupt masked, and on the way out in the opposite order.

Where code runs from the flash and a state powers it down, what must happen
once the flash is down goes in late hooks, defined with
`PBL_IDLE_HOOK_LATE_DEFINE()`. They belong to such states: the SoC calls
them from the innermost part of `enter()`, with
`pbl_idle_hooks_late_enter()` and `pbl_idle_hooks_late_exit()`. Their
functions, and all they call, run from RAM (`PBL_SECTION_RAM`); the kernel
checks the functions themselves at boot.

The kernel picks the deepest state that is not blocked and is worth
entering, then the hooks worth calling, programs the system clock event
device to fire their wake-up time ahead of the next timeout, enters the
state with every interrupt masked, then catches the tick count up from the
system clock source and goes back to a tick at a time.
Interrupts that woke the CPU run once the tick is back.

The time spent in each state is kept per state
(`pbl_idle_residency_us()`); `sys cpustats` prints it and the analytics
report it as `cpu_sleepN_pct`, N being the state index.
