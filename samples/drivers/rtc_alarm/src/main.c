/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <time.h>

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>

#define ALARM_S 3

static PBL_SEM_DEFINE(s_fired, 0, 1);

/* The alarm calls back from its interrupt: it only wakes main(). */
static void prv_alarm(const struct pbl_rtc *rtc, void *user_data) {
  pbl_sem_give(&s_fired);
}

int main(void) {
  time_t start, now;
  int ret;

  ret = pbl_rtc_get_time(RTC, &start);
  if (ret != 0) {
    fprintf(stderr, "rtc: read failed (%d)\n", ret);
    return ret;
  }

  ret = pbl_rtc_set_alarm(RTC, start + ALARM_S, prv_alarm, nullptr);
  if (ret != 0) {
    fprintf(stderr, "rtc: alarm failed (%d)\n", ret);
    return ret;
  }
  printf("rtc: alarm armed for %d s from now\n", ALARM_S);

  ret = pbl_sem_take(&s_fired, PBL_MSEC((ALARM_S + 2) * 1000));
  if (ret != 0) {
    fprintf(stderr, "rtc: no alarm (%d)\n", ret);
    return ret;
  }

  (void)pbl_rtc_get_time(RTC, &now);
  printf("rtc: alarm fired after %d s\n", (int)(now - start));
  printf("Sample done\n");

  return 0;
}
