/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "date.h"

bool date_util_is_leap_year(int year) {
  return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

int date_util_get_max_days_in_month(int month, bool is_leap_year) {
  int days;

  switch (month) {
    case 4: //April
    case 6: //June
    case 9: //September
    case 11: //November
    {
      days = 30;
      break;
    }
    case 2: // February
    {
      days = is_leap_year ? 29 : 28;
      break;
    }
    default:
    {
      // Jan, March, May, July, August, October, December
      days = 31;
      break;
    }
  }
  return days;
}
