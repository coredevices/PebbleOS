/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/regular_timer.h"

#include "drivers/rtc.h"
#include "pbl/os/mutex.h"
#include "pbl/services/new_timer/new_timer.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "pbl/util/math.h"
#include "util/time/time.h"

#include "FreeRTOS.h"
#include "portmacro.h"

#include <stdint.h>
#include <time.h>

PBL_LOG_MODULE_DEFINE(service_regular_timer, CONFIG_SERVICE_REGULAR_TIMER_LOG_LEVEL);

//! Don't let users modify the list while callbacks are occurring.
static PebbleMutex * s_callback_list_semaphore = 0;

//! The timer we use. One-shot, re-armed after every pass for the next due
//! callback instead of ticking at a fixed 1 Hz: multisecond callbacks are
//! credited with the elapsed seconds when the timer fires, so the wakeup
//! cadence collapses to the soonest-due callback (or the next minute boundary
//! for the minutes list) with no behavior change for the callbacks themselves.
static TimerID s_timer_id = TIMER_INVALID_ID;

static ListNode s_seconds_callbacks;
static ListNode s_minutes_callbacks;

//! Tick-time (monotonic, sleep-compensated) of the last processing pass,
//! kept aligned to whole elapsed seconds so fractional remainders carry over
//! instead of accumulating drift.
static RtcTicks s_last_run_ticks;

// Set to 90 seconds because we do eventually drift. Make it in the middle of a minute so we can
// be sure that it isn't due to drifting.
#define MISSING_MINUTE_CB_LOG_THRESHOLD_S 90
static time_t s_last_minute_fire_ts; // uses
static time_t s_last_minute_fired = -1; // Epoch-minute (utc / 60) we last fired on

static void timer_callback(void* data);

// -------------------------------------------------------------------------------------------
// Passed to list_find() to determine if a callback is already registered or not
static bool prv_callback_registered_filter(ListNode *found_node, void *data) {
  return (found_node == (ListNode *)data);
}

// -------------------------------------------------------------------------------------------
//! Seconds until the next seconds-list callback is due, or UINT32_MAX when
//! the list is empty. Assumes the callback list mutex is held.
static uint32_t prv_seconds_list_next_due(void) {
  uint32_t next = UINT32_MAX;

  for (ListNode* iter = list_get_next(&s_seconds_callbacks); iter != NULL;
       iter = list_get_next(iter)) {
    const RegularTimerInfo* reg_timer = (const RegularTimerInfo*) iter;
    next = MIN(next, MAX(reg_timer->private_count, 1));
  }

  return next;
}

// -------------------------------------------------------------------------------------------
//! (Re-)arm the timer for the next due callback. Assumes the callback list
//! mutex is held.
//!
//! Seconds-list deadlines are armed on the tick-clock grid anchored at
//! s_last_run_ticks — the same clock elapsed seconds are credited on — so a
//! fire can never land short of the seconds it is meant to credit. The
//! minutes list fires on wall-minute changes, so its deadline aims just past
//! the next wall-minute boundary; the wall and tick clocks are independent
//! oscillators on some boards, so an early landing is possible there and
//! costs one extra pass (the re-arm below then aims at the same boundary).
static void prv_arm_timer(void) {
  uint64_t delay_ms = UINT64_MAX;

  const uint32_t next_s = prv_seconds_list_next_due();
  if (next_s != UINT32_MAX) {
    const RtcTicks now_ticks = rtc_get_ticks();
    const RtcTicks target_ticks = s_last_run_ticks + (RtcTicks)next_s * RTC_TICKS_HZ;
    const RtcTicks delta = (target_ticks > now_ticks) ? (target_ticks - now_ticks) : 0;
    // Round up, +2ms bias: land past the grid point, never before it.
    delay_ms = (delta * 1000U + RTC_TICKS_HZ - 1) / RTC_TICKS_HZ + 2U;
  }

  if (list_get_next(&s_minutes_callbacks) != NULL) {
    time_t seconds;
    uint16_t milliseconds;
    rtc_get_time_ms(&seconds, &milliseconds);
    const uint32_t to_boundary_s =
        SECONDS_PER_MINUTE - (uint32_t)(seconds % SECONDS_PER_MINUTE);
    delay_ms = MIN(delay_ms, ((uint64_t)to_boundary_s * 1000U) - milliseconds + 2U);
  }

  if (delay_ms == UINT64_MAX) {
    new_timer_stop(s_timer_id);
    return;
  }

  new_timer_start(s_timer_id, (uint32_t)MAX(delay_ms, 1U), timer_callback, NULL, 0 /*flags*/);
}

// -------------------------------------------------------------------------------------------
//! Run due callbacks on a list after crediting elapsed whole seconds against
//! multisecond countdowns. For the minutes list, elapsed is always 1: minute
//! callbacks are credited per detected minute change, exactly as before.
static void do_callbacks(ListNode* list, uint32_t elapsed) {
  mutex_lock(s_callback_list_semaphore);

  for (ListNode* iter = list_get_next(list); iter != 0; ) {
    RegularTimerInfo* reg_timer = (RegularTimerInfo*) iter;

    reg_timer->private_count -= MIN(elapsed, reg_timer->private_count);
    if (reg_timer->private_count == 0) {
      reg_timer->private_count = reg_timer->private_reset_count;

      // Release the mutex while we execute the callback
      reg_timer->is_executing = true;
      mutex_unlock(s_callback_list_semaphore);
      reg_timer->cb(reg_timer->cb_data);
      mutex_lock(s_callback_list_semaphore);
      reg_timer->is_executing = false;

      // Get the next one to execute before we possibly remove this one
      iter = list_get_next(iter);

      // Did the caller want to remove this one?
      // NOTE: We do not support callers that free the memory for the regular timer structure
      // from their callback procedure!
      if (reg_timer->pending_delete) {
        list_remove(&reg_timer->list_node, NULL, NULL);
      }

    } else {
      iter = list_get_next(iter);
    }
  }

  mutex_unlock(s_callback_list_semaphore);
}

// -------------------------------------------------------------------------------------------
static void timer_callback(void* data) {
  // Whole seconds since the last pass, measured on the monotonic tick clock so
  // wall-clock changes can't stall or double-credit the countdowns. The
  // fractional remainder stays in s_last_run_ticks for the next pass.
  const RtcTicks now_ticks = rtc_get_ticks();
  uint32_t elapsed = (uint32_t)((now_ticks - s_last_run_ticks) / RTC_TICKS_HZ);
  if (elapsed > 0) {
    s_last_run_ticks += (RtcTicks)elapsed * RTC_TICKS_HZ;
    do_callbacks(&s_seconds_callbacks, elapsed);
  }

  // Fire minute callbacks when the minute changes (not just when tm_sec == 0).
  // This prevents missing callbacks when the RTC adjusts. Timezone and DST
  // offsets are whole minutes, so the UTC epoch-minute boundary is the local
  // one too.
  const time_t t = rtc_get_time();
  const time_t cur_minute = t / SECONDS_PER_MINUTE;

  bool should_fire_minute = false;
  if (s_last_minute_fired == -1) {
    // First run - initialize but don't fire
    s_last_minute_fired = cur_minute;
  } else if (s_last_minute_fired != cur_minute) {
    // Minute changed - fire callback
    should_fire_minute = true;
    s_last_minute_fired = cur_minute;
  }

  if (should_fire_minute) {
    // Keep the logging to detect large time jumps (multiple minutes skipped)
    const time_t now_ts = rtc_get_ticks() / configTICK_RATE_HZ;
    if ((now_ts - s_last_minute_fire_ts) > MISSING_MINUTE_CB_LOG_THRESHOLD_S) {
      PBL_LOG_WRN("Large time jump detected. Previous ts: %lu, Now ts: %lu",
              s_last_minute_fire_ts, now_ts);
    }
    s_last_minute_fire_ts = now_ts;

    do_callbacks(&s_minutes_callbacks, 1);
  }

  mutex_lock(s_callback_list_semaphore);
  prv_arm_timer();
  mutex_unlock(s_callback_list_semaphore);
}

// --------------------------------------------------------------------------------------------
void regular_timer_init(void) {
  PBL_ASSERTN(s_callback_list_semaphore == 0);

  s_callback_list_semaphore = mutex_create();

  s_last_run_ticks = rtc_get_ticks();
  s_timer_id = new_timer_create();
  // Armed lazily: the first registered callback arms the timer.
}

// -------------------------------------------------------------------------------------------
void regular_timer_add_multisecond_callback(RegularTimerInfo* cb, uint16_t seconds) {
  PBL_ASSERTN(s_callback_list_semaphore);

  mutex_lock(s_callback_list_semaphore);

  if ((list_get_next(&s_seconds_callbacks) == NULL) &&
      (list_get_next(&s_minutes_callbacks) == NULL)) {
    // Timer idle: the anchor is stale and no countdown depends on it.
    s_last_run_ticks = rtc_get_ticks();
  }

  // Seconds elapsed since the last crediting pass get credited to every
  // countdown at the next fire; pad the new countdown so it still waits the
  // full requested interval.
  const uint32_t uncredited =
      (uint32_t)((rtc_get_ticks() - s_last_run_ticks) / RTC_TICKS_HZ);

  cb->private_reset_count = seconds;
  cb->private_count = (uint16_t)MIN((uint32_t)seconds + uncredited, UINT16_MAX);

  // Only add to the list if not already registered
  if (!list_find(&s_seconds_callbacks, prv_callback_registered_filter, &cb->list_node)) {
    // better not be registered as a minute callback already
    PBL_ASSERTN(!list_find(&s_minutes_callbacks, prv_callback_registered_filter, &cb->list_node));
    cb->is_executing = false;
    cb->pending_delete = false;
    list_append(&s_seconds_callbacks, &cb->list_node);
  } else {
    // If it is marked for deletion, remove the deletion flag
    cb->pending_delete = false;
  }

  prv_arm_timer();

  mutex_unlock(s_callback_list_semaphore);
}

// --------------------------------------------------------------------------------------------
void regular_timer_add_seconds_callback(RegularTimerInfo* cb) {
  // special case for triggering each second
  regular_timer_add_multisecond_callback(cb, 1);
}

// --------------------------------------------------------------------------------------------
void regular_timer_add_multiminute_callback(RegularTimerInfo* cb, uint16_t minutes) {
  PBL_ASSERTN(s_callback_list_semaphore);

  mutex_lock(s_callback_list_semaphore);

  cb->private_reset_count = minutes;
  cb->private_count = minutes;

  if (!list_find(&s_minutes_callbacks, prv_callback_registered_filter, &cb->list_node)) {
    // better not be registered as a minute callback already
    PBL_ASSERTN(!list_find(&s_seconds_callbacks, prv_callback_registered_filter, &cb->list_node));
    cb->is_executing = false;
    cb->pending_delete = false;
    list_append(&s_minutes_callbacks, &cb->list_node);
  } else {
    // If it is marked for deletion, remove the deletion flag
    cb->pending_delete = false;
  }

  prv_arm_timer();

  mutex_unlock(s_callback_list_semaphore);
}

// -----------------------------------------------------------------------------------------
void regular_timer_add_minutes_callback(RegularTimerInfo* cb) {
  // special case for triggering each minute
  regular_timer_add_multiminute_callback(cb, 1);
}

// ------------------------------------------------------------------------------------------
static bool prv_regular_timer_is_scheduled(RegularTimerInfo *cb) {
  // Assumes mutex lock is already taken
  return (list_find(&s_seconds_callbacks, prv_callback_registered_filter, &cb->list_node) ||
          list_find(&s_minutes_callbacks, prv_callback_registered_filter, &cb->list_node));
}

// ------------------------------------------------------------------------------------------
bool regular_timer_is_scheduled(RegularTimerInfo *cb) {
  PBL_ASSERTN(s_callback_list_semaphore);

  mutex_lock(s_callback_list_semaphore);
  bool rv = prv_regular_timer_is_scheduled(cb);
  mutex_unlock(s_callback_list_semaphore);

  return (rv);
}

bool regular_timer_pending_deletion(RegularTimerInfo *cb) {
  return cb->pending_delete;
}

// ------------------------------------------------------------------------------------------
bool regular_timer_remove_callback(RegularTimerInfo* cb) {
  PBL_ASSERTN(s_callback_list_semaphore);
  bool timer_removed = false;
  mutex_lock(s_callback_list_semaphore);

  if (!prv_regular_timer_is_scheduled(cb)) {
    PBL_LOG_WRN("Timer not registered");
  } else {
    // If currently executing, mark for deletion. do_callbacks will delete it for us once
    // it completes.
    if (cb->is_executing) {
      cb->pending_delete = true;
    } else {
      list_remove(&cb->list_node, NULL, NULL);
      timer_removed = true;
      prv_arm_timer();
    }
  }

  mutex_unlock(s_callback_list_semaphore);
  return timer_removed;
}


// ---------------------------------------------------------------------------------------
// For Testing:

void regular_timer_deinit(void) {
  mutex_destroy((PebbleMutex *) s_callback_list_semaphore);
  s_callback_list_semaphore = NULL;
  new_timer_delete(s_timer_id);
  s_timer_id = TIMER_INVALID_ID;
}

static void prv_fire_callbacks(ListNode *list, uint16_t mod) {
  mutex_lock(s_callback_list_semaphore);
  ListNode* iter = list_get_next(list);
  while (iter) {
    RegularTimerInfo* reg_timer = (RegularTimerInfo*) iter;
    if (reg_timer->private_reset_count % mod == 0) {
      // Last one. Will trigger callback when do_callbacks() is called:
      reg_timer->private_count = 1;
    }
    iter = list_get_next(iter);
  }
  mutex_unlock(s_callback_list_semaphore);

  do_callbacks(list, 1);
}

void regular_timer_fire_seconds(uint8_t secs) {
  prv_fire_callbacks(&s_seconds_callbacks, secs);
}

void regular_timer_fire_minutes(uint8_t mins) {
  prv_fire_callbacks(&s_minutes_callbacks, mins);
}

static uint32_t prv_count(ListNode *list) {
  uint32_t count = 0;
  mutex_lock(s_callback_list_semaphore);
  // -1, because s_..._callbacks is a ListNode too
  count = list_count(list) - 1;
  mutex_unlock(s_callback_list_semaphore);
  return count;
}

uint32_t regular_timer_seconds_count(void) {
  return prv_count(&s_seconds_callbacks);
}

uint32_t regular_timer_minutes_count(void) {
  return prv_count(&s_minutes_callbacks);
}
