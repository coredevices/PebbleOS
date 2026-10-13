/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/analytics/analytics.h>

PBL_WEAK void pbl_analytics_init(void) {
}

PBL_WEAK void sys_pbl_analytics_set_signed(enum pbl_analytics_key key, int32_t signed_value) {
}

PBL_WEAK void sys_pbl_analytics_set_unsigned(enum pbl_analytics_key key, uint32_t unsigned_value) {
}

PBL_WEAK void sys_pbl_analytics_set_string(enum pbl_analytics_key key, const char *value) {
}

PBL_WEAK void sys_pbl_analytics_timer_start(enum pbl_analytics_key key) {
}

PBL_WEAK void sys_pbl_analytics_timer_stop(enum pbl_analytics_key key) {
}

PBL_WEAK void sys_pbl_analytics_add(enum pbl_analytics_key key, int32_t amount) {
}
