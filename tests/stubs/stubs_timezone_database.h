/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timezone_database.h"
#include "util/attributes.h"

int WEAK timezone_database_get_region_count(void) {
  return 0;
}

bool WEAK timezone_database_load_region_info(uint16_t region_id, TimezoneInfo *tz_info) {
  return false;
}

bool WEAK timezone_database_load_region_name(uint16_t region_id, char *region_name) {
  return false;
}

bool WEAK timezone_database_load_dst_rule(uint8_t dst_id, TimezoneDSTRule *start,
                                          TimezoneDSTRule *end) {
  return false;
}

int WEAK timezone_database_find_region_by_name(const char *region_name, int region_name_length) {
  return 0;
}
