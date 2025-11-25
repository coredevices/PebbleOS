/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_timer.h"

AppTimer* app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void* callback_data) {
  return NULL;
}

AppTimer* app_timer_register_repeatable(uint32_t timeout_ms,
                                        AppTimerCallback callback,
                                        void* callback_data,
                                        bool repeating) {
  return NULL;
}

bool app_timer_reschedule(AppTimer *timer, uint32_t new_timeout_ms) {
  return true;
}

void app_timer_cancel(AppTimer *timer) {
}
