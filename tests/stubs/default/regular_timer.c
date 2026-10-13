/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/regular_timer.h>

PBL_WEAK void regular_timer_init(void) {
}

PBL_WEAK void regular_timer_add_seconds_callback(RegularTimerInfo *cb) {
}

PBL_WEAK void regular_timer_add_multisecond_callback(RegularTimerInfo *cb, uint16_t seconds) {
}

PBL_WEAK void regular_timer_add_minutes_callback(RegularTimerInfo *cb) {
}

PBL_WEAK void regular_timer_add_multiminute_callback(RegularTimerInfo *cb, uint16_t minutes) {
}

PBL_WEAK bool regular_timer_remove_callback(RegularTimerInfo *cb) {
  return true;
}

PBL_WEAK bool regular_timer_is_scheduled(RegularTimerInfo *cb) {
  return true;
}
