/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdint.h>

//! @file trig.h
//!
//! @addtogroup Foundation
//! @{
//!   @addtogroup Math
//!   @{

//! The largest value that can result from a call to \ref sin_lookup or \ref cos_lookup.
//! For a code example, see the detailed description at the top of this chapter: \ref Math
#define TRIG_MAX_RATIO 0xffff

//! Angle value that corresponds to 360 degrees or 2 PI radians
//! @see \ref sin_lookup
//! @see \ref cos_lookup
#define TRIG_MAX_ANGLE 0x10000

//! Angle value that corresponds to 180 degrees or PI radians
//! @see \ref sin_lookup
//! @see \ref cos_lookup
#define TRIG_PI 0x8000

#define TRIG_FP 16

//! Converts from a fixed point value representation to the equivalent value in degrees
//! @see DEG_TO_TRIGANGLE
//! @see TRIG_MAX_ANGLE
#define TRIGANGLE_TO_DEG(trig_angle) (((trig_angle) * 360) / TRIG_MAX_ANGLE)

//! Converts from an angle in degrees to the equivalent fixed point value representation
//! @see TRIGANGLE_TO_DEG
//! @see TRIG_MAX_ANGLE
#define DEG_TO_TRIGANGLE(angle) (((angle) * TRIG_MAX_ANGLE) / 360)

//! Look-up the sine of the given angle from a pre-computed table.
//! @param angle The angle for which to compute the cosine.
//! The angle value is scaled linearly, such that a value of 0x10000 corresponds to 360 degrees or 2 PI radians.
int32_t sin_lookup(int32_t angle);

//! Look-up the cosine of the given angle from a pre-computed table.
//! This is equivalent to calling `sin_lookup(angle + TRIG_MAX_ANGLE / 4)`.
//! @param angle The angle for which to compute the cosine.
//! The angle value is scaled linearly, such that a value of 0x10000 corresponds to 360 degrees or 2 PI radians.
int32_t cos_lookup(int32_t angle);

//! Look-up the arctangent of a given x, y pair
//! The angle value is scaled linearly, such that a value of 0x10000 corresponds to 360 degrees or 2 PI radians.
int32_t atan2_lookup(int16_t y, int16_t x);

//! @internal
//! Normalize an angle to the range [0, TRIG_MAX_ANGLE]
uint32_t normalize_angle(int32_t angle);

//!   @} // group Math
//! @} // group Foundation
