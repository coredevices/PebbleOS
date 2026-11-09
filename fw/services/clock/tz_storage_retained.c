/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tz_storage.h"

#include <string.h>

#include <pbl/system/rtc_registers.h>
#include <pbl/util/size.h>

static const uint8_t s_regs[] = {
  RTC_TIMEZONE_ABBR_START, RTC_TIMEZONE_ABBR_END_TZID_DSTID,
  RTC_TIMEZONE_GMTOFFSET,  RTC_TIMEZONE_DST_START,
  RTC_TIMEZONE_DST_END,
};

static_assert(sizeof(TimezoneInfo) <= sizeof(s_regs) * sizeof(uint32_t),
              "TimezoneInfo must fit in the timezone registers");

void tz_storage_save(const TimezoneInfo *tz_info) {
  uint32_t raw[ARRAY_LENGTH(s_regs)] = {};

  memcpy(raw, tz_info, sizeof(*tz_info));
  for (size_t i = 0; i < ARRAY_LENGTH(s_regs); i++) {
    retained_write(s_regs[i], raw[i]);
  }
}

void tz_storage_load(TimezoneInfo *tz_info) {
  uint32_t raw[ARRAY_LENGTH(s_regs)];

  for (size_t i = 0; i < ARRAY_LENGTH(s_regs); i++) {
    raw[i] = retained_read(s_regs[i]);
  }
  memcpy(tz_info, raw, sizeof(*tz_info));
}

bool tz_storage_is_set(void) {
  // Set once an abbreviation, possibly "UNK", is stored.
  return retained_read(RTC_TIMEZONE_ABBR_START) != 0U;
}

void tz_storage_clear(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_regs); i++) {
    retained_write(s_regs[i], 0U);
  }
}
