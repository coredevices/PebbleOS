/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/time/time.h"

#include <stdbool.h>

bool WEAK alarm_get_next_enabled_alarm(time_t *next_alarm_time_out) {
  return false;
}

bool WEAK alarm_is_next_enabled_alarm_smart(void) {
  return false;
}
