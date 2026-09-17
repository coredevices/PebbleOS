/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include "util/time/time.h"

#include <stdbool.h>

bool PBL_WEAK alarm_get_next_enabled_alarm(time_t *next_alarm_time_out) {
  return false;
}

bool PBL_WEAK alarm_is_next_enabled_alarm_smart(void) {
  return false;
}
