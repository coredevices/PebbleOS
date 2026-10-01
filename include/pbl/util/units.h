/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define PBL_KIB(n) ((n) * 1024)
#define PBL_MIB(n) ((n) * 1024 * 1024)

#define PBL_MM_PER_M   1000
#define PBL_M_PER_KM   1000
#define PBL_M_PER_MILE 1609

#define PBL_USEC_PER_MSEC 1000
#define PBL_USEC_PER_SEC  1000000

#define PBL_MSEC_PER_SEC   1000
#define PBL_SEC_PER_MIN    60
#define PBL_MIN_PER_HOUR   60
#define PBL_HOUR_PER_DAY   24
#define PBL_DAY_PER_WEEK   7
#define PBL_MONTH_PER_YEAR 12
#define PBL_MSEC_PER_MIN   (PBL_MSEC_PER_SEC * PBL_SEC_PER_MIN)
#define PBL_SEC_PER_HOUR   (PBL_SEC_PER_MIN * PBL_MIN_PER_HOUR)
#define PBL_MIN_PER_DAY    (PBL_MIN_PER_HOUR * PBL_HOUR_PER_DAY)
#define PBL_SEC_PER_DAY    (PBL_SEC_PER_HOUR * PBL_HOUR_PER_DAY)
#define PBL_SEC_PER_WEEK   (PBL_SEC_PER_DAY * PBL_DAY_PER_WEEK)
