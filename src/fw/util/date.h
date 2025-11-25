/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define STDTIME_YEAR_OFFSET 1900

#define DAYS_PER_YEAR 365
#define MAX_DAYS_PER_YEAR 366

bool date_util_is_leap_year(int year);
int date_util_get_max_days_in_month(int month, bool is_leap_year);
