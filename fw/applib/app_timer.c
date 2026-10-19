/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/services/evented_timer.h>

#include <applib/app_logging.h>
#include <applib/app_timer.h>

//! @file fw/applib/app_timer.c
//!
//! Surprise! All this is is a dumb wrapper around evented_timer!

PBL_SYSCALL(AppTimer *, app_timer_register, (VAL, uint32_t, timeout_ms),
            (VAL, AppTimerCallback, callback), (VAL, void *, callback_data)) {
  // No need to check callback_data, we only dereference it in userspace anyway.
  return (AppTimer *)(uintptr_t)evented_timer_register(timeout_ms, false, callback, callback_data);
}

PBL_SYSCALL(AppTimer *, app_timer_register_repeatable, (VAL, uint32_t, timeout_ms),
            (VAL, AppTimerCallback, callback), (VAL, void *, callback_data),
            (VAL, bool, repeating)) {
  // No need to check callback_data, we only dereference it in userspace anyway.
  return (AppTimer *)(uintptr_t)evented_timer_register(timeout_ms, repeating, callback,
                                                       callback_data);
}

PBL_SYSCALL(bool, app_timer_reschedule, (VAL, AppTimer *, timer), (VAL, uint32_t, new_timeout_ms)) {
  if (from_user) {
    if (!evented_timer_exists((EventedTimerID)timer)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Timer %u does not exist", (unsigned)(uintptr_t)timer);
      return (false);
    }
    if (!evented_timer_is_current_task((EventedTimerID)timer)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid timer %u used in app_timer_reschedule",
              (unsigned)(uintptr_t)timer);
      pbl_syscall_fail();
    }
  }
  return evented_timer_reschedule((EventedTimerID)timer, new_timeout_ms);
}

PBL_SYSCALL(void, app_timer_cancel, (VAL, AppTimer *, timer)) {
  if (from_user) {
    if (!evented_timer_exists((EventedTimerID)timer)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Timer %u does not exist", (unsigned)(uintptr_t)timer);
      return;
    }
    if (!evented_timer_is_current_task((EventedTimerID)timer)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid timer %u used in app_timer_reschedule",
              (unsigned)(uintptr_t)timer);
      pbl_syscall_fail();
    }
  }
  evented_timer_cancel((EventedTimerID)timer);
}

PBL_SYSCALL(void *, app_timer_get_data, (VAL, AppTimer *, timer)) {
  if (from_user) {
    if (!evented_timer_exists((EventedTimerID)timer)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Timer %u does not exist", (unsigned)(uintptr_t)timer);
      return nullptr;
    }
    if (!evented_timer_is_current_task((EventedTimerID)timer)) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid timer %u used in app_timer_reschedule",
              (unsigned)(uintptr_t)timer);
      pbl_syscall_fail();
    }
  }

  return evented_timer_get_data((EventedTimerID)timer);
}
