# Task watchdog

`subsys/task_wdt/` is the software watchdog that catches stuck threads and
turns them into core dumps. Its public API is `include/pbl/task_wdt/task_wdt.h`.
It is modelled on the Zephyr task watchdog: a fixed pool of channels, each
with its own timeout and optional expiry callback, backing a single hardware
watchdog (`include/pbl/drivers/watchdog.h`).

## Model

- A **channel** watches one thread. `pbl_task_wdt_add()` binds it to the calling
  thread (or an explicit one) with a timeout (the system threads use
  `CONFIG_TASK_WDT_TIMEOUT_MS`); the thread, or something acting
  on its behalf, must call `pbl_task_wdt_feed()`, `pbl_task_wdt_feed_self()` or
  `pbl_task_wdt_feed_thread()` before the timeout runs out. `pbl_task_wdt_feed_all()`
  exists for long, lock-holding operations such as flash erases.
- A dedicated **watchdog thread** at the highest priority wakes every
  `CONFIG_TASK_WDT_CHECK_PERIOD_MS`, compares every channel against the
  kernel uptime, and feeds the hardware watchdog when it decides not to
  reset. Its sleep also bounds how long the SoC may stay in deep sleep, so
  the hardware watchdog is kept alive without SoC-specific timers. A thread
  spinning with interrupts disabled, or an interrupt handler that never
  returns, starves the watchdog thread too; the hardware watchdog is the
  backstop for those.
- When a channel **expires**, the watchdog thread logs the stuck thread's
  name, saved PC and LR, stores them in the reboot reason
  (`RebootReasonCode_Watchdog`, with the fed and active channel masks in
  `data8`), and calls the channel callback. The callback may try to unblock
  the thread and returns a pointer naming the work it was doing, which lands
  in `stuck_task_callback`. It runs again on every check while the channel
  stays expired.
- After `CONFIG_TASK_WDT_GRACE_MS` of a channel staying expired the system
  resets through `reset_due_to_software_failure()`, so a core dump is
  written. Without `CONFIG_WATCHDOG` it logs instead and keeps running. A
  channel fed again within the grace period clears the reboot reason and
  logs the recovery.
- `pbl_task_wdt_suspend()` keeps every channel fed for a bounded time (or until
  `pbl_task_wdt_resume()`), for phases such as boot or a long settings-file
  compaction where stalls are expected.

## Users

- `src/fw/main.c` starts the watchdog, suspends it for the first 30 s of
  boot, adds the NewTimers channel (fed from a regular timer, which proves
  the timer thread still runs callbacks) and the KernelMain channel, which
  the launcher event loop feeds.
- `src/fw/services/system_task/service.c` owns the KernelBackground
  channel. Its callback throttles the app thread to the idle priority for a
  short time when the system task is ready to run but starved.
- `src/fw/console/pulse2.c` owns the PULSE channel.
- Flash, filesystem and console code feeds the calling thread's channel from
  loops that legitimately run for seconds.

## Tests

`tests/subsys/task_wdt/` runs the subsystem on the POSIX kernel port with
virtual time; see `docs/development/testing.md` for how to run it.
