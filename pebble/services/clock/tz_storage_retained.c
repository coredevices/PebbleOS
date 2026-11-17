/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tz_storage.h"

#include <stddef.h>
#include <string.h>

#include <pbl/system/rtc_registers.h>
#include <pbl/util/size.h>

static const uint8_t s_regs[] = {
  RTC_TIMEZONE_ABBR_START, RTC_TIMEZONE_ABBR_END_TZID_DSTID,
  RTC_TIMEZONE_GMTOFFSET,  RTC_TIMEZONE_DST_START,
  RTC_TIMEZONE_DST_END,    RTC_TIMEZONE_DST_START_HI,
  RTC_TIMEZONE_DST_END_HI,
};

// The fields up to tm_gmtoff are stored as raw words, the DST times as 32 bits.
#define TZ_RAW_WORDS 3
static_assert(offsetof(TimezoneInfo, tm_gmtoff) + sizeof(int32_t) ==
                  TZ_RAW_WORDS * sizeof(uint32_t),
              "TimezoneInfo layout changed");

void tz_storage_save(const TimezoneInfo *tz_info) {
  uint32_t raw[TZ_RAW_WORDS];

  memcpy(raw, tz_info, sizeof(raw));
  for (size_t i = 0; i < TZ_RAW_WORDS; i++) {
    retained_write(s_regs[i], raw[i]);
  }
  retained_write(RTC_TIMEZONE_DST_START, (uint32_t)tz_info->dst_start);
  retained_write(RTC_TIMEZONE_DST_START_HI, (uint32_t)((uint64_t)tz_info->dst_start >> 32));
  retained_write(RTC_TIMEZONE_DST_END, (uint32_t)tz_info->dst_end);
  retained_write(RTC_TIMEZONE_DST_END_HI, (uint32_t)((uint64_t)tz_info->dst_end >> 32));
}

void tz_storage_load(TimezoneInfo *tz_info) {
  uint32_t raw[TZ_RAW_WORDS];

  for (size_t i = 0; i < TZ_RAW_WORDS; i++) {
    raw[i] = retained_read(s_regs[i]);
  }
  *tz_info = (TimezoneInfo){};
  memcpy(tz_info, raw, sizeof(raw));
  tz_info->dst_start = (time_t)(((uint64_t)retained_read(RTC_TIMEZONE_DST_START_HI) << 32) |
                                retained_read(RTC_TIMEZONE_DST_START));
  tz_info->dst_end = (time_t)(((uint64_t)retained_read(RTC_TIMEZONE_DST_END_HI) << 32) |
                              retained_read(RTC_TIMEZONE_DST_END));
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
