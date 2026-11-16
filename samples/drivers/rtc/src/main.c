/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <time.h>

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/delay.h>

/* 2026-01-01 00:00:00 UTC */
#define SAMPLE_TIME ((time_t)1767225600)
#define WAIT_S      2

static void prv_log_time(const char *what, time_t t) {
  struct tm tm;
  char buf[32];

  gmtime_r(&t, &tm);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  printf("rtc: %s %s UTC\n", what, buf);
}

int main(void) {
  time_t saved, now;
  int ret;

  /* The RTC counts UTC seconds since the epoch, through resets. */
  ret = pbl_rtc_get_time(RTC, &saved);
  if (ret != 0) {
    fprintf(stderr, "rtc: read failed (%d)\n", ret);
    return ret;
  }
  prv_log_time("now", saved);

  ret = pbl_rtc_set_time(RTC, SAMPLE_TIME);
  if (ret != 0) {
    fprintf(stderr, "rtc: write failed (%d)\n", ret);
    return ret;
  }
  prv_log_time("set to", SAMPLE_TIME);

  pbl_sleep_ms(WAIT_S * 1000);

  ret = pbl_rtc_get_time(RTC, &now);
  if (ret != 0) {
    fprintf(stderr, "rtc: read failed (%d)\n", ret);
    return ret;
  }
  prv_log_time("read back", now);
  printf("rtc: %d s later it counted on: %s\n", WAIT_S,
         now >= SAMPLE_TIME + WAIT_S - 1 && now <= SAMPLE_TIME + WAIT_S + 1 ? "yes" : "no");

  /* Put the time back, plus the time it spent on the sample date. */
  ret = pbl_rtc_set_time(RTC, saved + (now - SAMPLE_TIME));
  if (ret != 0) {
    fprintf(stderr, "rtc: write failed (%d)\n", ret);
    return ret;
  }
  printf("rtc: restored\n");

  printf("Sample done\n");

  return 0;
}
