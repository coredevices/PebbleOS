/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <time.h>

struct pbl_tm *pbl_override_localtime(const time_t *timep) {
  static struct pbl_tm local_tm;
  pbl_localtime_r(timep, &local_tm);
  return &local_tm;
}

struct pbl_tm *pbl_override_gmtime(const time_t *timep) {
  static struct pbl_tm local_tm;
  pbl_gmtime_r(timep, &local_tm);
  return &local_tm;
}

time_t pbl_override_mktime(struct pbl_tm *tb) {
  return pbl_mktime(tb);
}
