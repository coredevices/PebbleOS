/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "shell/normal/watchface_metrics.h"

void watchface_metrics_init(void) {}

void watchface_metrics_start(const Uuid *uuid) {}

void watchface_metrics_stop(void) {}

uint32_t watchface_metrics_get_current_time(void) {
  return 0;
}
