/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <memfault/metrics/metrics.h>

void analytics_impl_init(void);

#define PBL_ANALYTICS_IMPL_SET_SIGNED(key_name, signed_value) \
  MEMFAULT_METRIC_SET_SIGNED(key_name, signed_value)

#define PBL_ANALYTICS_IMPL_SET_UNSIGNED(key_name, unsigned_value) \
  MEMFAULT_METRIC_SET_UNSIGNED(key_name, unsigned_value)

#define PBL_ANALYTICS_IMPL_SET_STRING(key_name, value) MEMFAULT_METRIC_SET_STRING(key_name, value)

#define PBL_ANALYTICS_IMPL_TIMER_START(key_name) MEMFAULT_METRIC_TIMER_START(key_name)

#define PBL_ANALYTICS_IMPL_TIMER_STOP(key_name) MEMFAULT_METRIC_TIMER_STOP(key_name)

#define PBL_ANALYTICS_IMPL_ADD(key_name, amount) MEMFAULT_METRIC_ADD(key_name, amount)