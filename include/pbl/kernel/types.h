/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pbl/kernel/backend.h"

/**
 * @defgroup kernel_types Time and priorities
 * @ingroup kernel
 * @brief Ticks, timeouts and thread priorities shared by every kernel object.
 *
 * Blocking calls take a @ref pbl_timeout_t built with one of the timeout macros, so ticks and
 * milliseconds cannot be mixed up:
 *
 * @code{.c}
 * pbl_sem_take(&sem, PBL_NO_WAIT);    // poll
 * pbl_sem_take(&sem, PBL_MSEC(100));  // wait up to 100 ms
 * pbl_sem_take(&sem, PBL_FOREVER);    // wait indefinitely
 * @endcode
 * @{
 */

/** @brief Kernel tick count. Wraps around; compare differences, not absolute values. */
typedef uint32_t pbl_tick_t;

/** @brief Tick rate in Hz, from @c CONFIG_KERNEL_TICK_HZ. Matches the RTC tick rate. */
#define PBL_TICK_HZ CONFIG_KERNEL_TICK_HZ
/** @brief Tick count that stands for an infinite timeout. */
#define PBL_TICK_FOREVER UINT32_MAX

/**
 * @brief Timeout of a blocking call.
 *
 * A distinct type so ticks and milliseconds cannot be mixed; build it with @ref PBL_NO_WAIT,
 * @ref PBL_FOREVER, @ref PBL_TICKS, @ref PBL_MSEC or @ref PBL_SEC.
 */
typedef struct {
  /** Ticks to wait, 0 for no wait or @ref PBL_TICK_FOREVER for no limit. */
  pbl_tick_t ticks;
} pbl_timeout_t;

/** @brief Do not wait: fail with @c -EBUSY if the call would block. */
#define PBL_NO_WAIT ((pbl_timeout_t){.ticks = 0})
/** @brief Wait without a time limit. */
#define PBL_FOREVER ((pbl_timeout_t){.ticks = PBL_TICK_FOREVER})
/**
 * @brief Timeout in ticks.
 *
 * @param t Number of ticks.
 */
#define PBL_TICKS(t) ((pbl_timeout_t){.ticks = (t)})
/**
 * @brief Timeout in milliseconds, rounded down to whole ticks.
 *
 * @param ms Number of milliseconds.
 */
#define PBL_MSEC(ms) ((pbl_timeout_t){.ticks = pbl_ms_to_ticks(ms)})
/**
 * @brief Timeout in seconds.
 *
 * @param s Number of seconds.
 */
#define PBL_SEC(s) PBL_MSEC((s) * 1000U)

/**
 * @brief Convert milliseconds to ticks, rounding down.
 *
 * @param ms Milliseconds.
 * @return Ticks.
 */
pbl_tick_t pbl_ms_to_ticks(uint32_t ms);

/**
 * @brief Convert ticks to milliseconds, rounding down.
 *
 * @param ticks Ticks.
 * @return Milliseconds.
 */
uint32_t pbl_ticks_to_ms(pbl_tick_t ticks);

/**
 * @brief Check whether a timeout is @ref PBL_FOREVER.
 *
 * @param t Timeout.
 * @return true if @p t never expires.
 */
static inline bool pbl_timeout_is_forever(pbl_timeout_t t) {
  return t.ticks == PBL_TICK_FOREVER;
}
/**
 * @brief Check whether a timeout is @ref PBL_NO_WAIT.
 *
 * @param t Timeout.
 * @return true if @p t does not wait.
 */
static inline bool pbl_timeout_is_no_wait(pbl_timeout_t t) {
  return t.ticks == 0;
}

/** @brief Thread priority. Higher value = more urgent. */
typedef uint8_t pbl_prio_t;
/** @brief Lowest priority, the one of the idle thread. */
#define PBL_PRIO_IDLE ((pbl_prio_t)0)
/** @brief Highest priority, @c CONFIG_KERNEL_NUM_PRIORITIES - 1. */
#define PBL_PRIO_MAX ((pbl_prio_t)(CONFIG_KERNEL_NUM_PRIORITIES - 1))

/** @} */

struct pbl_thread;
