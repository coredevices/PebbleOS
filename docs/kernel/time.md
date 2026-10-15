# Time and delays

The kernel keeps one monotonic clock, the tick count, advanced by the tick
interrupt and by `pbl_idle_slept()` after tickless sleep. `pbl/kernel/time.h`
reads it and the CPU cycle counter; `pbl/kernel/delay.h` waits on them.

## Uptime

`pbl_uptime_ticks()` and `pbl_uptime_ms()` count from the scheduler start in
64 bits, so they do not wrap during the life of a device. The 32-bit
`pbl_tick_t` remains for relative timeouts only.

`pbl_cycle_get_32()` reads the CPU cycle counter (DWT `CYCCNT` on Cortex-M) at
`pbl_cycles_per_sec()`, the current CPU clock. Use it to time short sections:
it wraps within seconds and stops while the CPU sleeps.

## Waiting

| Need                               | Use                  | Resolution | Context        |
|------------------------------------|----------------------|------------|----------------|
| A few microseconds, or no thread   | `pbl_busy_wait_us()` | cycles     | anywhere       |
| Microseconds, being late is fine   | `pbl_sleep_us()`     | one tick   | thread         |
| Milliseconds and longer            | `pbl_sleep_ms()`     | one tick   | thread         |
| A timeout or a deadline in ticks   | `pbl_thread_sleep()` | one tick   | thread         |
| A condition, spinning              | `PBL_POLL_BUSY_US()` | cycles     | anywhere       |
| A condition, sleeping              | `PBL_POLL_SLEEP_MS()`| one tick   | thread         |

The `pbl_sleep_*()` and `pbl_busy_wait_us()` calls wait at least the requested
time, which is what hardware timings from a datasheet need: sleeps round up to
whole ticks and add one for the tick already in progress. `pbl_thread_sleep()`
counts tick boundaries instead, so its first tick may be partial.

The polling macros return 0 once the condition holds and `-ETIMEDOUT`
otherwise, evaluating it once more after the timeout so that preemption cannot
turn a ready condition into a false timeout, as Linux `read_poll_timeout()`
does.

Busy waiting keeps the CPU awake and starves lower priority threads; past a
tick or so, sleep instead.
