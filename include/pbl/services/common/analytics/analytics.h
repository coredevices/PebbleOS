/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(ANALYTICS_IMPL_MEMFAULT)
#include "memfault/analytics_impl.h"
#else
#include "null/analytics_impl.h"
#endif

static inline void analytics_init(void) {
  analytics_impl_init();
}

void analytics_external_update(void);

#define PBL_ANALYTICS_SET_SIGNED(key_name, signed_value) \
  PBL_ANALYTICS_IMPL_SET_SIGNED(key_name, signed_value)

#define PBL_ANALYTICS_SET_UNSIGNED(key_name, unsigned_value) \
  PBL_ANALYTICS_IMPL_SET_UNSIGNED(key_name, unsigned_value)

#define PBL_ANALYTICS_SET_STRING(key_name, value) PBL_ANALYTICS_IMPL_SET_STRING(key_name, value)

#define PBL_ANALYTICS_TIMER_START(key_name) PBL_ANALYTICS_IMPL_TIMER_START(key_name)

#define PBL_ANALYTICS_TIMER_STOP(key_name) PBL_ANALYTICS_IMPL_TIMER_STOP(key_name)

#define PBL_ANALYTICS_ADD(key_name, amount) PBL_ANALYTICS_IMPL_ADD(key_name, amount)
