/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/list.h>

/**
 * @defgroup services_regular_timer Regular timers
 * @ingroup services
 * @brief Callbacks run every N seconds or every N minutes.
 *
 * All regular timers share one timer aligned to the RTC second boundary, so periodic work wakes
 * the system at the same time. Second callbacks run shortly after each second boundary, minute
 * callbacks when the wall-clock minute changes. Callbacks run on the NewTimers task and must not
 * block for long.
 *
 * The caller owns the RegularTimerInfo, which must stay valid while registered:
 *
 * @code{.c}
 * static void prv_tick(void *data) {
 *   // Runs on the NewTimers task every 5 seconds.
 * }
 *
 * static RegularTimerInfo s_timer = {
 *   .cb = prv_tick,
 * };
 *
 * regular_timer_add_multisecond_callback(&s_timer, 5);
 * ...
 * regular_timer_remove_callback(&s_timer);
 * @endcode
 * @{
 */

/**
 * @brief Regular timer callback.
 *
 * @param data RegularTimerInfo::cb_data.
 */
typedef void (*RegularTimerCallback)(void *data);

/** @brief Regular timer registration, owned by the caller. */
typedef struct RegularTimerInfo {
  /** @cond INTERNAL_HIDDEN */
  ListNode list_node;
  /** @endcond */
  /** Callback to run. */
  RegularTimerCallback cb;
  /** Context passed to the callback. */
  void *cb_data;

  /** @cond INTERNAL_HIDDEN */
  uint16_t private_reset_count;
  uint16_t private_count;
  bool is_executing;
  bool pending_delete;
  bool private_due;
  /** @endcond */
} RegularTimerInfo;

/** @brief Initialize the service and start the shared timer. */
void regular_timer_init(void);

/**
 * @brief Run a callback every second.
 *
 * @param cb Timer to register.
 */
void regular_timer_add_seconds_callback(RegularTimerInfo *cb);

/**
 * @brief Run a callback every @p seconds seconds.
 *
 * Can also change the period of a registered seconds timer, from inside or outside its callback.
 * Re-adding a timer pending deletion cancels the deletion. A timer cannot be both a seconds and
 * a minutes timer.
 *
 * @param cb Timer to register.
 * @param seconds Period in seconds.
 */
void regular_timer_add_multisecond_callback(RegularTimerInfo *cb, uint16_t seconds);

/**
 * @brief Run a callback every minute.
 *
 * @param cb Timer to register.
 */
void regular_timer_add_minutes_callback(RegularTimerInfo *cb);

/**
 * @brief Run a callback every @p minutes minutes.
 *
 * Can also change the period of a registered minutes timer, from inside or outside its callback.
 * Re-adding a timer pending deletion cancels the deletion. A timer cannot be both a seconds and
 * a minutes timer.
 *
 * @param cb Timer to register.
 * @param minutes Period in minutes.
 */
void regular_timer_add_multiminute_callback(RegularTimerInfo *cb, uint16_t minutes);

/**
 * @brief Unregister a seconds or minutes timer.
 *
 * If the callback is running, the timer is only marked for deletion and is removed when the
 * callback returns. When called from the callback itself, the RegularTimerInfo must not be freed
 * until the callback has returned.
 *
 * @param cb Timer to unregister.
 * @retval true The timer was removed.
 * @retval false The timer was not registered, or is running and was marked for deletion.
 */
bool regular_timer_remove_callback(RegularTimerInfo *cb);

/**
 * @brief Check whether a timer is registered.
 *
 * @param cb Timer to check.
 * @return True if registered, including when pending deletion.
 */
bool regular_timer_is_scheduled(RegularTimerInfo *cb);

/**
 * @brief Check whether a timer is pending deletion.
 *
 * A timer is pending deletion when it was removed while its callback was running.
 *
 * @param cb Timer to check.
 * @return True if pending deletion.
 */
bool regular_timer_pending_deletion(RegularTimerInfo *cb);

/** @brief Stop the shared timer, for tests. */
void regular_timer_deinit(void);

/**
 * @brief Fire the seconds timers whose period is a multiple of @p secs, for tests.
 *
 * @param secs Period divisor.
 */
void regular_timer_fire_seconds(uint8_t secs);

/**
 * @brief Fire the minutes timers whose period is a multiple of @p mins, for tests.
 *
 * @param mins Period divisor.
 */
void regular_timer_fire_minutes(uint8_t mins);

/**
 * @brief Count the registered seconds timers, for tests.
 *
 * @return Number of timers.
 */
uint32_t regular_timer_seconds_count(void);

/**
 * @brief Count the registered minutes timers, for tests.
 *
 * @return Number of timers.
 */
uint32_t regular_timer_minutes_count(void);

/** @} */
