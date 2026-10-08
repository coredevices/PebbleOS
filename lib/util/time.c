/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/time.h>
#include <pbl/util/units.h>

static const uint8_t s_mon_lengths[2][PBL_MONTH_PER_YEAR] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static const uint16_t s_year_lengths[2] = {365, 366};

bool pbl_time_is_leap_year(int year) {
  return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

int pbl_time_days_in_month(int month, bool is_leap_year) {
  return s_mon_lengths[is_leap_year ? 1 : 0][month - 1];
}

void pbl_time_split_seconds(uint32_t seconds, uint32_t *days, uint32_t *hours, uint32_t *minutes,
                            uint32_t *secs) {
  *days = seconds / PBL_SEC_PER_DAY;
  seconds -= *days * PBL_SEC_PER_DAY;

  *hours = seconds / PBL_SEC_PER_HOUR;
  seconds -= *hours * PBL_SEC_PER_HOUR;

  *minutes = seconds / PBL_SEC_PER_MIN;
  seconds -= *minutes * PBL_SEC_PER_MIN;

  *secs = seconds;
}

int pbl_time_display_hour(int hour, bool is_24h) {
  return is_24h ? hour : (hour + 12 - 1) % 12 + 1;
}

int pbl_time_minute_of_day_adjust(int minute, int delta) {
  minute += delta;
  if (minute < 0) {
    minute += PBL_MIN_PER_DAY;
  } else if (minute >= PBL_MIN_PER_DAY) {
    minute -= PBL_MIN_PER_DAY;
  }
  return minute;
}

void pbl_time_breakdown(time_t t, struct tm *tm) {
  int32_t days = t / PBL_SEC_PER_DAY;
  int32_t rem = t % PBL_SEC_PER_DAY;
  while (rem < 0) {
    rem += PBL_SEC_PER_DAY;
    --days;
  }
  while (rem >= PBL_SEC_PER_DAY) {
    rem -= PBL_SEC_PER_DAY;
    ++days;
  }

  tm->tm_hour = (int)(rem / PBL_SEC_PER_HOUR);
  rem %= PBL_SEC_PER_HOUR;
  tm->tm_min = (int)(rem / PBL_SEC_PER_MIN);
  tm->tm_sec = (int)(rem % PBL_SEC_PER_MIN);

  if ((tm->tm_wday = ((PBL_EPOCH_WDAY + days) % PBL_DAY_PER_WEEK)) < 0) {
    tm->tm_wday += PBL_DAY_PER_WEEK;
  }

  int y = PBL_EPOCH_YEAR;
  int yleap;
  if (days >= 0) {
    for (;;) {
      yleap = pbl_time_is_leap_year(y);
      if (days < s_year_lengths[yleap]) {
        break;
      }
      y++;
      days -= s_year_lengths[yleap];
    }
  } else {
    do {
      --y;
      yleap = pbl_time_is_leap_year(y);
      days += s_year_lengths[yleap];
    } while (days < 0);
  }

  tm->tm_year = y - PBL_TM_YEAR_ORIGIN;
  tm->tm_yday = days;
  const uint8_t *ip = s_mon_lengths[yleap];
  for (tm->tm_mon = 0; days >= ip[tm->tm_mon]; ++tm->tm_mon) {
    days -= ip[tm->tm_mon];
  }
  tm->tm_mday = days + 1;
}
