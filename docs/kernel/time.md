# Time and delays

The kernel keeps one monotonic clock, the tick count, derived from the
system clock source, and the wall clock on top of it. `pbl/kernel/time.h`
reads them and the CPU cycle counter; `pbl/kernel/delay.h` waits on them.

## System timer

Two devices from `pbl/drivers/timer.h` keep time, defined by the SoC's timer
driver as `pbl_system_clocksource` and `pbl_system_clockevent`:

- The clock source is a free-running counter. The kernel extends it to 64
  bits and converts it to ticks in fixed point, so its rate need not divide
  the tick rate and may change at run time, as a calibrated RC oscillator's
  does, without time jumping.
- The clock event device raises an interrupt once, a given number of its
  cycles from now. While threads run, the kernel programs it for the next
  tick; when the CPU idles, for the next timeout. Ticks are counted from
  the clock source on every event, so a late or missed interrupt loses no
  time.

Drivers know nothing about ticks: a clock source reads its counter, a clock
event device programs its compare register.

| SoC | Clock source | Clock event device |
| --- | --- | --- |
| nRF52 | RTC1, 32.768 kHz | RTC1 compare |
| SF32LB52 | LPTIM1, calibrated RC10K | LPTIM1 compare, wakes from deep sleep |
| QEMU | QEMU RTC 1 kHz counter | SysTick |
| POSIX | host monotonic clock | host thread |

## Uptime

`pbl_uptime_ticks()` and `pbl_uptime_ms()` count from the scheduler start in
64 bits, so they do not wrap during the life of a device. The 32-bit
`pbl_tick_t` remains for relative timeouts only.

`pbl_cycle_get_32()` reads the CPU cycle counter (DWT `CYCCNT` on Cortex-M) at
`pbl_cycles_per_sec()`, the current CPU clock. Use it to time short sections:
it wraps within seconds and stops while the CPU sleeps.

## Wall clock

`pbl_realtime()` and `pbl_realtime_ms()` return UTC time since the epoch,
kept as an offset from the uptime with microsecond resolution: the wall clock
runs at the clock source rate and only jumps when `pbl_realtime_set()` sets
it. The kernel keeps it in RAM only. The clock service restores it at boot
from the board's RTC (`pbl/drivers/rtc.h`), and writes it back whenever the
time is set and every `CONFIG_SERVICE_CLOCK_RTC_SAVE_INTERVAL` seconds.

| SoC | RTC |
| --- | --- |
| nRF52 | Retained RAM: the time last saved, which does not count while off |
| SF32LB52 | Calendar RTC on the calibrated RC10K |
| QEMU | QEMU RTC, following the host clock |
| POSIX | Host clock |

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
