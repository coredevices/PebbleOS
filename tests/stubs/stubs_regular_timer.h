/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/regular_timer.h"

void regular_timer_init(void) { }

void regular_timer_add_seconds_callback(RegularTimerInfo* cb) { }

void regular_timer_add_multisecond_callback(RegularTimerInfo* cb, uint16_t seconds) { }

void regular_timer_add_minutes_callback(RegularTimerInfo* cb) { }

void regular_timer_add_multiminute_callback(RegularTimerInfo* cb, uint16_t minutes) { }

bool regular_timer_remove_callback(RegularTimerInfo* cb) {
  return true;
}

bool regular_timer_is_scheduled(RegularTimerInfo *cb) {
  return true;
}
