/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/app_timer.h>

PBL_WEAK AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback,
                                      void *callback_data) {
  return nullptr;
}

PBL_WEAK AppTimer *app_timer_register_repeatable(uint32_t timeout_ms, AppTimerCallback callback,
                                                 void *callback_data, bool repeating) {
  return nullptr;
}

PBL_WEAK bool app_timer_reschedule(AppTimer *timer, uint32_t new_timeout_ms) {
  return true;
}

PBL_WEAK void app_timer_cancel(AppTimer *timer) {
}
