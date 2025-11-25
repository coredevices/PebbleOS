/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/animation.h"
#include "util/math.h"
#include "util/math_fixed.h"

//! @file animation_timing.h

//! Converts normalized time to a segmented-delayed fractional duration. The duration is computed
//! by multiplying with duration_fraction which is less than 1. The delay segment is calculated by
//! taking the non-animating duration given by the complete normalized duration minus the
//! fractional duration. The non-animating duration is then divided by the number of segments
//! specified to obtain the amount of time to delay an animation item for each index. The zeroth
//! index has no delay, and each subsequent item receives a multiple of delay segments to wait.
//! @param time_normalized the normalized time between 0 and ANIMATION_NORMALIZED_MAX inclusive
//! @param index determines how many delay segments to wait until timing starts
//! @param num_segments the number of segments to partition non-animating duration by
//! @param duration_fraction a Fixed_S32_16 fraction between 0 and 1 of the animation duration time
//! @returns the segmented time
AnimationProgress animation_timing_segmented(AnimationProgress time_normalized, int32_t index,
                                             uint32_t num_segments, Fixed_S32_16 duration_fraction);

//! Converts normalized time to a timing based on a curve defined by a table.
//! @param time_normalized the normalized time between 0 and ANIMATION_NORMALIZED_MAX inclusive
//! @param table a curve with entries eased from 0 to ANIMATION_NORMALIZED_MAX
//! @param num_entries number of entries in the table
AnimationProgress animation_timing_interpolate(
    AnimationProgress time_normalized, const uint16_t *table, size_t num_entries);

AnimationProgress animation_timing_interpolate32(
    AnimationProgress time_normalized, const int32_t *table, size_t num_entries);

//! Converts normalized time to a timing based on a specified curve
//! @param time_normalized the normalized time between 0 and ANIMATION_NORMALIZED_MAX inclusive
//! @param curve a system animation curve to convert to. @see AnimationCurve
//! @returns the curved time
AnimationProgress animation_timing_curve(AnimationProgress time_normalized, AnimationCurve curve);

static inline AnimationProgress animation_timing_clip(AnimationProgress time_normalized) {
    return CLIP(time_normalized, 0, ANIMATION_NORMALIZED_MAX);
}

//! Rescales a given time as with respect to a given interval
AnimationProgress animation_timing_scaled(AnimationProgress time_normalized,
                                          AnimationProgress interval_start,
                                          AnimationProgress interval_end);
