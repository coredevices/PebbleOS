/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(ANALYTICS_IMPL_MEMFAULT)
#include "memfault/analytics_impl.h"
#else
#include "null/analytics_impl.h"
#endif

#if defined(ANALYTICS_DIRECT_HEARTBEAT)
#include "direct/heartbeat_v70.h"
#endif

static inline void analytics_init(void) {
  analytics_impl_init();
#if defined(ANALYTICS_DIRECT_HEARTBEAT)
  v70_heartbeat_init();
#endif
}

void analytics_external_update(void);

#if defined(ANALYTICS_DIRECT_HEARTBEAT)

#define PBL_ANALYTICS_SET_SIGNED(key_name, signed_value) do { \
    PBL_ANALYTICS_IMPL_SET_SIGNED(key_name, signed_value); \
    v70_shadow_set_signed(V70_METRIC_##key_name, (signed_value)); \
  } while (0)

#define PBL_ANALYTICS_SET_UNSIGNED(key_name, unsigned_value) do { \
    PBL_ANALYTICS_IMPL_SET_UNSIGNED(key_name, unsigned_value); \
    v70_shadow_set_unsigned(V70_METRIC_##key_name, (unsigned_value)); \
  } while (0)

#define PBL_ANALYTICS_SET_STRING(key_name, value) do { \
    PBL_ANALYTICS_IMPL_SET_STRING(key_name, value); \
    v70_shadow_set_string(V70_METRIC_##key_name, (value)); \
  } while (0)

#define PBL_ANALYTICS_TIMER_START(key_name) do { \
    PBL_ANALYTICS_IMPL_TIMER_START(key_name); \
    v70_shadow_timer_start(V70_METRIC_##key_name); \
  } while (0)

#define PBL_ANALYTICS_TIMER_STOP(key_name) do { \
    PBL_ANALYTICS_IMPL_TIMER_STOP(key_name); \
    v70_shadow_timer_stop(V70_METRIC_##key_name); \
  } while (0)

#define PBL_ANALYTICS_ADD(key_name, amount) do { \
    PBL_ANALYTICS_IMPL_ADD(key_name, amount); \
    v70_shadow_add(V70_METRIC_##key_name, (amount)); \
  } while (0)

#else

#define PBL_ANALYTICS_SET_SIGNED(key_name, signed_value) \
  PBL_ANALYTICS_IMPL_SET_SIGNED(key_name, signed_value)

#define PBL_ANALYTICS_SET_UNSIGNED(key_name, unsigned_value) \
  PBL_ANALYTICS_IMPL_SET_UNSIGNED(key_name, unsigned_value)

#define PBL_ANALYTICS_SET_STRING(key_name, value) PBL_ANALYTICS_IMPL_SET_STRING(key_name, value)

#define PBL_ANALYTICS_TIMER_START(key_name) PBL_ANALYTICS_IMPL_TIMER_START(key_name)

#define PBL_ANALYTICS_TIMER_STOP(key_name) PBL_ANALYTICS_IMPL_TIMER_STOP(key_name)

#define PBL_ANALYTICS_ADD(key_name, amount) PBL_ANALYTICS_IMPL_ADD(key_name, amount)

#endif
