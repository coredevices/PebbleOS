/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/animation_timing.h"
#include "util/attributes.h"

AnimationProgress WEAK animation_timing_segmented(
    AnimationProgress time_normalized, int32_t index, uint32_t num_segments,
    Fixed_S32_16 duration_fraction) {
  return time_normalized;
}

AnimationProgress WEAK animation_timing_curve(AnimationProgress time_normalized,
                                              AnimationCurve curve) {
  return time_normalized;
}

AnimationProgress WEAK animation_timing_scaled(AnimationProgress time_normalized,
                                               AnimationProgress interval_start,
                                               AnimationProgress interval_end) {
  return interval_end;
}

