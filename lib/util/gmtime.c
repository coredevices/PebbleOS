/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include <time.h>

#include <pbl/util/time.h>

struct pbl_tm *pbl_gmtime_r(const time_t *timep, struct pbl_tm *result) {
  result->tm_gmtoff = 0;
  result->tm_isdst = 0;
  strncpy(result->tm_zone, "UTC", PBL_TZ_LEN);
  result->tm_zone[PBL_TZ_LEN - 1] = '\0';
  pbl_time_breakdown(*timep, result);
  return result;
}
