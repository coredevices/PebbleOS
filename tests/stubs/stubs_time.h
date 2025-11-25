/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/time/time.h"

time_t WEAK time_util_get_midnight_of(time_t ts) {
  return 0;
}

bool WEAK time_util_range_spans_day(time_t start, time_t end, time_t start_of_day) {
  return false;
}
