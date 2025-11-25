/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "util/time/time.h"


struct tm *pbl_override_localtime(const time_t *timep) {
  static struct tm local_tm;
  localtime_r(timep, &local_tm);
  return &local_tm;
}

struct tm *pbl_override_gmtime(const time_t *timep) {
  static struct tm local_tm;
  gmtime_r(timep, &local_tm);
  return &local_tm;
}

time_t pbl_override_mktime(struct tm *tb) {
  return mktime(tb);
}

