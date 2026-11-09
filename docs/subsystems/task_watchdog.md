# Task watchdog

`subsys/task_wdt/` is the software watchdog that catches stuck threads and
turns them into core dumps, enabled by `CONFIG_TASK_WDT`. Its public API is
`include/pbl/task_wdt/task_wdt.h`.
It is a fixed pool of channels, each with its own timeout and optional expiry
callback, backing a single hardware watchdog (`include/pbl/drivers/watchdog.h`).

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
  name, saved PC and LR, and calls the channel callback. The callback may
  try to unblock the thread and returns a pointer naming the work it was
  doing. It runs again on every check while the channel stays expired. The
  stall is then reported with `pbl_task_wdt_stalled()`, which the firmware
  overrides to store it in the reboot reason (`RebootReasonCode_Watchdog`,
  with the fed and active channel masks in `data8`, and the work in
  `stuck_task_callback`).
- After `CONFIG_TASK_WDT_GRACE_MS` of a channel staying expired, counted
  from the check that first found it expired, `pbl_task_wdt_expired()`
  resets the system: it panics by default, and the firmware overrides it
  with `reset_due_to_software_failure()`, so a core dump is written. That
  check never resets, so the callback always gets the grace period to
  recover the thread, and the next check runs once the grace period is over
  instead of a full check period later. Without `CONFIG_WATCHDOG` it logs
  instead and keeps running. A channel fed again within the grace period is
  reported with `pbl_task_wdt_recovered()`, with which the firmware clears
  the reboot reason, and logs the recovery.
- A thread waiting for work (`pbl_task_wdt_set_waiting()`) does not expire
  while it is blocked. Once woken it must run within its timeout, counted
  from the check that first finds it runnable, so a waiting thread starved
  by a busier, higher-priority one still expires.
- `pbl_task_wdt_add_workq()` watches a work queue's thread: it waits while the
  queue is idle and its channel is fed after each item, so each item gets the
  whole timeout.
- `pbl_task_wdt_suspend()` keeps every channel fed for a bounded time (or until
  `pbl_task_wdt_resume()`), for phases such as boot or a long settings-file
  compaction where stalls are expected.

## Users

- `pebble/main.c` starts the watchdog, suspends it for the first 30 s of
  boot, watches the high priority system work queue with
  `pbl_task_wdt_add_workq()`, and adds the KernelMain channel, which the
  launcher event loop feeds.
- `pebble/process_management/app_throttle.c` watches the background system work
  queue. Its callback throttles the app thread to the idle priority for a
  short time when that queue is ready to run but starved.
- `subsys/bluetooth/init.c` owns the NimBLE host channel, waiting while the
  host has no events to run.
- Flash, filesystem and console code feeds the calling thread's channel from
  loops that legitimately run for seconds.

## Tests

`tests/subsys/task_wdt/` runs the subsystem on the POSIX kernel port with
virtual time; see {doc}`../development/tests/unit` for how to run it.

The QEMU boards carry a real hardware watchdog (the CMSDK APB watchdog,
driven by `drivers/watchdog/qemu.c`), so both paths can be tried in
the emulator: `wdt stall main` ends in a core dump, and `wdt stall irq`
ends in a hardware reset that the next boot reports as a watchdog reset.
