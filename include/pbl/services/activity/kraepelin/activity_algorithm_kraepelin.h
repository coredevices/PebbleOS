/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/units.h>

/**
 * @addtogroup services_activity_kraepelin
 * @{
 */

/** @brief Divisor applied to the raw light sensor reading stored in minute records. */
#define ALG_RAW_LIGHT_SENSOR_DIVIDE_BY 16

/**
 * @brief Sleep sessions ending after this minute of the day (9pm) are primary sleep, not naps.
 */
#define ALG_PRIMARY_EVENING_MINUTE (21 * PBL_MIN_PER_HOUR)
/**
 * @brief Sleep sessions starting before this minute of the day (12pm) are primary sleep, not
 * naps.
 */
#define ALG_PRIMARY_MORNING_MINUTE (12 * PBL_MIN_PER_HOUR)

/**
 * @brief Maximum length of a nap, in minutes.
 *
 * Longer sleep sessions outside the primary range are primary sleep.
 */
#define ALG_MAX_NAP_MINUTES (3 * PBL_MIN_PER_HOUR)

/**
 * @brief Hours of past minute data processed to compute today's sleep.
 *
 * A sleep session ending after midnight counts as today's, so it may have started more than 24
 * hours ago.
 */
#define ALG_SLEEP_HISTORY_HOURS_FOR_TODAY 36

/** @} */
