/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "analytics.h"

#include <stdint.h>

/**
 * @defgroup services_analytics_backend Analytics backends
 * @ingroup services_analytics
 * @brief Interface implemented by analytics backends.
 * @{
 */

/**
 * @brief Operations of an analytics backend.
 *
 * The analytics service validates the key and forwards every metric update to each backend.
 */
struct pbl_analytics_backend_ops {
  /** Set a signed metric. */
  void (*set_signed)(enum pbl_analytics_key key, int32_t signed_value);
  /** Set an unsigned metric. */
  void (*set_unsigned)(enum pbl_analytics_key key, uint32_t unsigned_value);
  /** Set a string metric. */
  void (*set_string)(enum pbl_analytics_key key, const char *value);
  /** Start a timer metric. */
  void (*timer_start)(enum pbl_analytics_key key);
  /** Stop a timer metric. */
  void (*timer_stop)(enum pbl_analytics_key key);
  /** Add to an integer metric. */
  void (*add)(enum pbl_analytics_key key, int32_t amount);
};

/** @} */
