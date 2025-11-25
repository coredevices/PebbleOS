/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_state.h"

#include <time.h>

struct tm *app_state_get_gmtime_tm(void) {
  static struct tm gmtime_tm = {0};
  return &gmtime_tm;
}
struct tm *app_state_get_localtime_tm(void) {
  static struct tm localtime_tm = {0};
  return &localtime_tm;
}
char *app_state_get_localtime_zone(void) {
  static char localtime_zone[TZ_LEN] = {0};
  return localtime_zone;
}

LocaleInfo *app_state_get_locale_info(void) {
  static LocaleInfo locale_info = {0};
  return &locale_info;
}
