/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/types.h>

/**
 * @defgroup kernel_timer Timers
 * @ingroup kernel
 * @brief One-shot software timers expiring in the tick interrupt.
 *
 * The callback runs in the tick ISR, so it must not block. Timers keep the idle thread from
 * sleeping past their expiry. All functions may be called from ISRs, including from a callback
 * to restart its own timer.
 *
 * @code{.c}
 * static void prv_expired(struct pbl_timer *timer) {
 *   struct foo *foo = container_of(timer, struct foo, timer);
 *   ...
 * }
 *
 * pbl_timer_init(&foo->timer, prv_expired);
 * pbl_timer_start(&foo->timer, PBL_MSEC(40));
 * @endcode
 * @{
 */

struct pbl_timer;

/** @brief Timer callback, run in the tick ISR. */
typedef void (*pbl_timer_fn_t)(struct pbl_timer *timer);

/** @brief Software timer. */
struct pbl_timer {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_timer *next;
  pbl_tick_t expires;
  bool active;
  /** @endcond */
  /** Callback. */
  pbl_timer_fn_t fn;
};

/**
 * @brief Static initializer for a timer.
 *
 * @param _fn Callback.
 */
#define PBL_TIMER_INITIALIZER(_fn) {.fn = (_fn)}

/**
 * @brief Initialize a timer.
 *
 * @param[out] timer Timer.
 * @param fn Callback.
 */
void pbl_timer_init(struct pbl_timer *timer, pbl_timer_fn_t fn);

/**
 * @brief Start a timer, or restart it if it is active.
 *
 * @param timer Timer.
 * @param delay Time until the callback runs, at least one tick; must not be @ref PBL_FOREVER.
 */
void pbl_timer_start(struct pbl_timer *timer, pbl_timeout_t delay);

/**
 * @brief Stop a timer. Does nothing if it is not active.
 *
 * @param timer Timer.
 */
void pbl_timer_stop(struct pbl_timer *timer);

/**
 * @brief Check whether a timer is active.
 *
 * @param timer Timer.
 * @return True from pbl_timer_start() until it expires or is stopped.
 */
bool pbl_timer_is_active(const struct pbl_timer *timer);

/** @} */
