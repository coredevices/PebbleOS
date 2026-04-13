/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define PBL_ANALYTICS_KEY(key_name) PBL_ANALYTICS_KEY__##key_name

enum pbl_analytics_key {
#define PBL_ANALYTICS_METRIC_DEFINE_UNSIGNED(key) \
    PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_SIGNED(key) \
    PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_SCALED_UNSIGNED(key, scale) \
    PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_SCALED_SIGNED(key, scale) \
    PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_TIMER(key) \
    PBL_ANALYTICS_KEY(key),
#define PBL_ANALYTICS_METRIC_DEFINE_STRING(key, len) \
    PBL_ANALYTICS_KEY(key),
  #include "analytics.def"
#undef PBL_ANALYTICS_METRIC_DEFINE_UNSIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_SIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_SCALED_UNSIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_SCALED_SIGNED
#undef PBL_ANALYTICS_METRIC_DEFINE_TIMER
#undef PBL_ANALYTICS_METRIC_DEFINE_STRING
  PBL_ANALYTICS_KEY_COUNT,
};

void pbl_analytics_init(void);

void pbl_analytics_set_signed(enum pbl_analytics_key key, int32_t signed_value);

void pbl_analytics_set_unsigned(enum pbl_analytics_key key, uint32_t unsigned_value);

void pbl_analytics_set_string(enum pbl_analytics_key key, const char *value);

void pbl_analytics_timer_start(enum pbl_analytics_key key);

void pbl_analytics_timer_stop(enum pbl_analytics_key key);

void pbl_analytics_add(enum pbl_analytics_key key, int32_t amount);

#define PBL_ANALYTICS_SET_SIGNED(key_name, signed_value) \
  pbl_analytics_set_signed(PBL_ANALYTICS_KEY(key_name), signed_value)

#define PBL_ANALYTICS_SET_UNSIGNED(key_name, unsigned_value) \
  pbl_analytics_set_unsigned(PBL_ANALYTICS_KEY(key_name), unsigned_value)

#define PBL_ANALYTICS_SET_STRING(key_name, value) \
  pbl_analytics_set_string(PBL_ANALYTICS_KEY(key_name), value)

#define PBL_ANALYTICS_TIMER_START(key_name) \
  pbl_analytics_timer_start(PBL_ANALYTICS_KEY(key_name))

#define PBL_ANALYTICS_TIMER_STOP(key_name) \
  pbl_analytics_timer_stop(PBL_ANALYTICS_KEY(key_name))

#define PBL_ANALYTICS_ADD(key_name, amount) \
  pbl_analytics_add(PBL_ANALYTICS_KEY(key_name), amount)
