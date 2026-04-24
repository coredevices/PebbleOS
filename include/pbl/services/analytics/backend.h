/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "analytics.h"

#include <stdint.h>

struct pbl_analytics_backend_ops {
  void (*set_signed)(enum pbl_analytics_key key, int32_t signed_value);
  void (*set_unsigned)(enum pbl_analytics_key key, uint32_t unsigned_value);
  void (*set_string)(enum pbl_analytics_key key, const char *value);
  void (*timer_start)(enum pbl_analytics_key key);
  void (*timer_stop)(enum pbl_analytics_key key);
  void (*add)(enum pbl_analytics_key key, int32_t amount);
};