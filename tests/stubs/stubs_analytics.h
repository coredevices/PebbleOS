/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/analytics/analytics.h"

void pbl_analytics_init(void) {}

void sys_pbl_analytics_set_signed(enum pbl_analytics_key key, int32_t signed_value) {
  (void)key;
  (void)signed_value;
}

void sys_pbl_analytics_set_unsigned(enum pbl_analytics_key key, uint32_t unsigned_value) {
  (void)key;
  (void)unsigned_value;
}

void sys_pbl_analytics_set_string(enum pbl_analytics_key key, const char *value) {
  (void)key;
  (void)value;
}

void sys_pbl_analytics_timer_start(enum pbl_analytics_key key) {
  (void)key;
}

void sys_pbl_analytics_timer_stop(enum pbl_analytics_key key) {
  (void)key;
}

void sys_pbl_analytics_add(enum pbl_analytics_key key, int32_t amount) {
  (void)key;
  (void)amount;
}
