/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdint.h>

/**
 * @defgroup util_trig Trigonometry
 * @ingroup util
 * @brief Fixed-point trigonometry helpers.
 *
 * Angles are scaled so that @ref TRIG_MAX_ANGLE is a full turn. Entities shared with the app SDK
 * are documented in the SDK's Math group.
 */

/**
 * @addtogroup Foundation
 * @{
 */

/**
 * @addtogroup Math
 * @{
 */

/**
 * The largest value that can result from a call to \ref sin_lookup or \ref cos_lookup.
 * For a code example, see the detailed description at the top of this chapter: \ref Math
 */
#define TRIG_MAX_RATIO 0xffff

/**
 * Angle value that corresponds to 360 degrees or 2 PI radians
 * @see \ref sin_lookup
 * @see \ref cos_lookup
 */
#define TRIG_MAX_ANGLE 0x10000

/**
 * @ingroup util_trig
 * @brief Angle value that corresponds to 180 degrees or PI radians.
 * @see sin_lookup
 * @see cos_lookup
 */
#define TRIG_PI 0x8000

/**
 * @ingroup util_trig
 * @brief Number of fractional bits of the fixed-point ratio used by atan2_lookup().
 */
#define TRIG_FP 16

/**
 * Converts from a fixed point value representation to the equivalent value in degrees
 * @see DEG_TO_TRIGANGLE
 * @see TRIG_MAX_ANGLE
 * @param trig_angle Angle, scaled so that TRIG_MAX_ANGLE is 360 degrees.
 * @return Angle in degrees.
 */
#define TRIGANGLE_TO_DEG(trig_angle) (((trig_angle) * 360) / TRIG_MAX_ANGLE)

/**
 * Converts from an angle in degrees to the equivalent fixed point value representation
 * @see TRIGANGLE_TO_DEG
 * @see TRIG_MAX_ANGLE
 * @param angle Angle in degrees.
 * @return Angle, scaled so that TRIG_MAX_ANGLE is 360 degrees.
 */
#define DEG_TO_TRIGANGLE(angle) (((angle) * TRIG_MAX_ANGLE) / 360)

/**
 * @brief Look-up the sine of the given angle from a pre-computed table.
 * @param angle The angle for which to compute the sine.
 * The angle value is scaled linearly, such that a value of 0x10000 corresponds to 360 degrees or 2
 * PI radians.
 * @return Sine of @p angle, scaled so that 1 is TRIG_MAX_RATIO.
 */
int32_t sin_lookup(int32_t angle);

/**
 * @brief Look-up the cosine of the given angle from a pre-computed table.
 *
 * This is equivalent to calling `sin_lookup(angle + TRIG_MAX_ANGLE / 4)`.
 * @param angle The angle for which to compute the cosine.
 * The angle value is scaled linearly, such that a value of 0x10000 corresponds to 360 degrees or 2
 * PI radians.
 * @return Cosine of @p angle, scaled so that 1 is TRIG_MAX_RATIO.
 */
int32_t cos_lookup(int32_t angle);

/**
 * @brief Look-up the arctangent of a given x, y pair
 *
 * The angle value is scaled linearly, such that a value of 0x10000 corresponds to 360 degrees or 2
 * PI radians.
 * @param y Y coordinate.
 * @param x X coordinate.
 * @return Angle of the point (x, y) counter-clockwise from the positive x axis, from 0 up to but
 *         not including TRIG_MAX_ANGLE.
 */
int32_t atan2_lookup(int16_t y, int16_t x);

/**
 * @ingroup util_trig
 * @brief Normalize an angle to the range [0, TRIG_MAX_ANGLE].
 *
 * @param angle Angle, scaled so that @ref TRIG_MAX_ANGLE is a full turn; may be negative.
 * @return Equivalent angle; @ref TRIG_MAX_ANGLE only for negative multiples of a full turn.
 */
uint32_t normalize_angle(int32_t angle);

/** @} */

/** @} */
