/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup util_units Units
 * @ingroup util
 * @brief Size, distance and time unit conversion factors.
 * @{
 */

/**
 * @brief Convert kibibytes to bytes.
 *
 * @param n Number of KiB.
 */
#define PBL_KIB(n) ((n) * 1024)
/**
 * @brief Convert mebibytes to bytes.
 *
 * @param n Number of MiB.
 */
#define PBL_MIB(n) ((n) * 1024 * 1024)

/** @brief Millimeters per meter. */
#define PBL_MM_PER_M 1000
/** @brief Meters per kilometer. */
#define PBL_M_PER_KM 1000
/** @brief Meters per mile, rounded. */
#define PBL_M_PER_MILE 1609

/** @brief Microseconds per millisecond. */
#define PBL_USEC_PER_MSEC 1000
/** @brief Microseconds per second. */
#define PBL_USEC_PER_SEC 1000000

/** @brief Milliseconds per second. */
#define PBL_MSEC_PER_SEC 1000
/** @brief Seconds per minute. */
#define PBL_SEC_PER_MIN 60
/** @brief Minutes per hour. */
#define PBL_MIN_PER_HOUR 60
/** @brief Hours per day. */
#define PBL_HOUR_PER_DAY 24
/** @brief Days per week. */
#define PBL_DAY_PER_WEEK 7
/** @brief Months per year. */
#define PBL_MONTH_PER_YEAR 12
/** @brief Milliseconds per minute. */
#define PBL_MSEC_PER_MIN (PBL_MSEC_PER_SEC * PBL_SEC_PER_MIN)
/** @brief Seconds per hour. */
#define PBL_SEC_PER_HOUR (PBL_SEC_PER_MIN * PBL_MIN_PER_HOUR)
/** @brief Minutes per day. */
#define PBL_MIN_PER_DAY (PBL_MIN_PER_HOUR * PBL_HOUR_PER_DAY)
/** @brief Seconds per day. */
#define PBL_SEC_PER_DAY (PBL_SEC_PER_HOUR * PBL_HOUR_PER_DAY)
/** @brief Seconds per week. */
#define PBL_SEC_PER_WEEK (PBL_SEC_PER_DAY * PBL_DAY_PER_WEEK)

/** @} */
