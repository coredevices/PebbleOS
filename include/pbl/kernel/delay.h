/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <errno.h>
#include <stdint.h>

#include <pbl/kernel/time.h>

/**
 * @defgroup kernel_delay Delays
 * @ingroup kernel
 * @brief Waiting for a minimum amount of time, sleeping or spinning.
 *
 * Every call waits at least the requested time. Sleeping blocks the calling thread with tick
 * resolution, so other threads run and the CPU may enter low power; it is only allowed from a
 * thread, with the scheduler running and unlocked. Busy waiting spins with sub-microsecond
 * resolution and works anywhere, including ISRs and before the scheduler starts, but burns
 * power and starves lower priority threads: keep it to a few hundred microseconds.
 *
 * | Need                                  | Use                  |
 * |---------------------------------------|----------------------|
 * | A few microseconds, or no thread      | pbl_busy_wait_us()   |
 * | Microseconds, being late is fine      | pbl_sleep_us()       |
 * | Milliseconds and longer               | pbl_sleep_ms()       |
 * | A timeout or a deadline               | pbl_thread_sleep()   |
 * | A condition, spinning                 | PBL_POLL_BUSY_US()   |
 * | A condition, sleeping                 | PBL_POLL_SLEEP_MS()  |
 *
 * @code{.c}
 * prv_reg_write(REG_RESET, 1);
 * pbl_busy_wait_us(10);  // t_RST >= 10 us
 * prv_reg_write(REG_CTRL, CTRL_EN);
 * pbl_sleep_ms(5);       // t_STARTUP >= 5 ms
 *
 * int rc = PBL_POLL_SLEEP_MS(prv_reg_read(REG_STATUS) & STATUS_READY, 1, 100);
 * if (rc < 0) {
 *   return rc;  // -ETIMEDOUT
 * }
 * @endcode
 * @{
 */

/**
 * @brief Spin for at least a number of microseconds.
 *
 * Counts CPU cycles at the CPU clock read on entry; changing the CPU clock meanwhile skews it.
 * Interrupts and preemption are not masked and add to the delay. Privileged only.
 *
 * @param us Microseconds to wait.
 */
void pbl_busy_wait_us(uint32_t us);

/**
 * @brief Sleep for at least a number of milliseconds.
 *
 * Rounds up to whole ticks and adds one for the partial tick in progress, so the thread may sleep
 * up to a tick longer than asked, plus scheduling latency.
 *
 * @param ms Milliseconds to sleep; 0 yields instead.
 */
void pbl_sleep_ms(uint32_t ms);

/**
 * @brief Sleep for at least a number of microseconds.
 *
 * Like pbl_sleep_ms(): the resolution is one tick, so any non-zero value sleeps at least a tick.
 *
 * @param us Microseconds to sleep; 0 yields instead.
 */
void pbl_sleep_us(uint32_t us);

/**
 * @brief Spin until a condition holds or a timeout expires.
 *
 * Evaluates @p cond, then every @p delay_us of busy waiting, timing out on the cycle counter.
 * @p cond is evaluated once more after the timeout expires, so a slow evaluation or an interrupt
 * cannot cause a false timeout. Usable wherever pbl_busy_wait_us() is.
 *
 * @param cond Condition, evaluated repeatedly.
 * @param delay_us Microseconds to spin between evaluations, 0 to re-evaluate immediately.
 * @param timeout_us Microseconds to give up after; at most one second.
 * @return 0 once @p cond holds, -ETIMEDOUT otherwise.
 */
#define PBL_POLL_BUSY_US(cond, delay_us, timeout_us)                             \
  ({                                                                             \
    const uint32_t _pbl_start = pbl_cycle_get_32();                              \
    const uint32_t _pbl_limit =                                                  \
        (uint32_t)(((uint64_t)(timeout_us) * pbl_cycles_per_sec()) / 1000000U);  \
    int _pbl_rc;                                                                 \
    for (;;) {                                                                   \
      const bool _pbl_expired = (pbl_cycle_get_32() - _pbl_start) >= _pbl_limit; \
      if (cond) {                                                                \
        _pbl_rc = 0;                                                             \
        break;                                                                   \
      }                                                                          \
      if (_pbl_expired) {                                                        \
        _pbl_rc = -ETIMEDOUT;                                                    \
        break;                                                                   \
      }                                                                          \
      pbl_busy_wait_us(delay_us);                                                \
    }                                                                            \
    _pbl_rc;                                                                     \
  })

/**
 * @brief Sleep until a condition holds or a timeout expires.
 *
 * Evaluates @p cond, then after every pbl_sleep_ms() of @p sleep_ms, timing out on the uptime.
 * @p cond is evaluated once more after the timeout expires. Thread context only.
 *
 * @param cond Condition, evaluated repeatedly.
 * @param sleep_ms Milliseconds to sleep between evaluations, 0 to yield.
 * @param timeout_ms Milliseconds to give up after.
 * @return 0 once @p cond holds, -ETIMEDOUT otherwise.
 */
#define PBL_POLL_SLEEP_MS(cond, sleep_ms, timeout_ms)                          \
  ({                                                                           \
    const uint64_t _pbl_start = pbl_uptime_ms();                               \
    int _pbl_rc;                                                               \
    for (;;) {                                                                 \
      const bool _pbl_expired = (pbl_uptime_ms() - _pbl_start) > (timeout_ms); \
      if (cond) {                                                              \
        _pbl_rc = 0;                                                           \
        break;                                                                 \
      }                                                                        \
      if (_pbl_expired) {                                                      \
        _pbl_rc = -ETIMEDOUT;                                                  \
        break;                                                                 \
      }                                                                        \
      pbl_sleep_ms(sleep_ms);                                                  \
    }                                                                          \
    _pbl_rc;                                                                   \
  })

/** @} */
