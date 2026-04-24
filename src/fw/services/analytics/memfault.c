/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/analytics/backend.h"

#include <memfault/metrics/metrics.h>

extern void memfault_platform_boot_early(void);
extern void memfault_platform_heartbeat(void);

static const MemfaultMetricId s_pbl_to_memfault[] = {
#define _PBL_TO_MEMFAULT(key) [PBL_ANALYTICS_KEY(key)] = MEMFAULT_METRICS_KEY(key),

#define PBL_ANALYTICS_METRIC_DEFINE_UNSIGNED(key) _PBL_TO_MEMFAULT(key)
#define PBL_ANALYTICS_METRIC_DEFINE_SIGNED(key) _PBL_TO_MEMFAULT(key)
#define PBL_ANALYTICS_METRIC_DEFINE_SCALED_UNSIGNED(key, scale) _PBL_TO_MEMFAULT(key)
#define PBL_ANALYTICS_METRIC_DEFINE_SCALED_SIGNED(key, scale) _PBL_TO_MEMFAULT(key)
#define PBL_ANALYTICS_METRIC_DEFINE_TIMER(key) _PBL_TO_MEMFAULT(key)
#define PBL_ANALYTICS_METRIC_DEFINE_STRING(key, len) _PBL_TO_MEMFAULT(key)
  #include "pbl/services/analytics/analytics.def"
};

void pbl_analytics__memfault_init(void) {
  memfault_platform_boot_early();
}

void pbl_analytics__memfault_heartbeat(void) {
  memfault_platform_heartbeat();
}

static void prv_set_signed(enum pbl_analytics_key key, int32_t signed_value) {
  memfault_metrics_heartbeat_set_signed(s_pbl_to_memfault[key], signed_value);
}

static void prv_set_unsigned(enum pbl_analytics_key key, uint32_t unsigned_value) {
  memfault_metrics_heartbeat_set_unsigned(s_pbl_to_memfault[key], unsigned_value);
}

static void prv_set_string(enum pbl_analytics_key key, const char *value) {
  memfault_metrics_heartbeat_set_string(s_pbl_to_memfault[key], value);
}

static void prv_timer_start(enum pbl_analytics_key key) {
  memfault_metrics_heartbeat_timer_start(s_pbl_to_memfault[key]);
}

static void prv_timer_stop(enum pbl_analytics_key key) {
  memfault_metrics_heartbeat_timer_stop(s_pbl_to_memfault[key]);
}

static void prv_add(enum pbl_analytics_key key, int32_t amount) {
  memfault_metrics_heartbeat_add(s_pbl_to_memfault[key], amount);
}

const struct pbl_analytics_backend_ops pbl_analytics__memfault_ops = {
  .set_signed = prv_set_signed,
  .set_unsigned = prv_set_unsigned,
  .set_string = prv_set_string,
  .timer_start = prv_timer_start,
  .timer_stop = prv_timer_stop,
  .add = prv_add,
};