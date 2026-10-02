/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_activity_activity_calculators Activity calculators
 * @ingroup services_activity
 * @brief Distance and calorie estimates from steps and the user profile.
 *
 * Use the height, weight, gender and age stored in the activity prefs.
 * @{
 */

/**
 * @brief Estimate the distance covered by a number of steps.
 *
 * The stride length depends on the user's height and on the cadence.
 *
 * @param steps Steps taken.
 * @param ms Time taken, in milliseconds.
 * @return Distance, in millimeters; 0 if @p steps or @p ms is 0.
 */
uint32_t activity_private_compute_distance_mm(uint32_t steps, uint32_t ms);

/**
 * @brief Estimate the active calories burned covering a distance.
 *
 * Rates above 120 m/min are considered running, which doubles the cost per meter.
 *
 * @param distance_mm Distance, in millimeters.
 * @param ms Time taken, in milliseconds.
 * @return Calories (1/1000 kcal); 0 if @p distance_mm or @p ms is 0.
 */
uint32_t activity_private_compute_active_calories(uint32_t distance_mm, uint32_t ms);

/**
 * @brief Estimate the resting calories burned over a period.
 *
 * Uses the Mifflin-St Jeor resting metabolic rate.
 *
 * @param elapsed_minutes Period, in minutes.
 * @return Calories (1/1000 kcal).
 */
uint32_t activity_private_compute_resting_calories(uint32_t elapsed_minutes);

/** @} */
