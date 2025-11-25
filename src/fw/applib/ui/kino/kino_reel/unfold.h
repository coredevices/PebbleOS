/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scale_segmented.h"

#include "applib/ui/kino/kino_reel.h"

#define UNFOLD_DEFAULT_POINT_DURATION \
  Fixed_S32_16(FIXED_S32_16_ONE.raw_value / 6)

#define UNFOLD_DEFAULT_EFFECT_DURATION \
  Fixed_S32_16(3 * FIXED_S32_16_ONE.raw_value / 4)

#define UNFOLD_DEFAULT_NUM_DELAY_GROUPS 3
#define UNFOLD_DEFAULT_GROUP_DELAY Fixed_S32_16(FIXED_S32_16_ONE.raw_value * 3 / 2)

#define UNFOLD_DOT_SIZE_PX SCALE_SEGMENTED_DOT_SIZE_PX
#define UNFOLD_DOT_SIZE SCALE_SEGMENTED_DOT_SIZE

//! A KinoReel that can perform a one-stage or two-stage unfold with or
//! without a deflation and bounce back effect. The effects can be simultaneous or independent and
//! are achieved by overlapping two invocations of \ref gdraw_command_image_scale_segmented_to.
//! The unfold effect is achieved by using a GPointIndexLookup created by
//! \ref gdraw_command_image_create_index_lookup_by_angle.
//! @param from_reel KinoReel to display and animate.
//! @param take_ownership true if this KinoReel will free `image` when destroyed.
//! @param screen_frame Position and size of the parent KinoLayer in absolute drawing coordinates.
//! @param angle Angle to start the unfold effect from, where TRIG_MAX_ANGLE represents 2 pi.
//! If 0, a random angle will be used.
//! @param num_delay_groups Number of distinct point groups that will move together in time.
//! @param group_delay Amount of additional delay to add between each group proportional to the
//! animation duration of one group
//! @return a scale segmented KinoReel with an angle-based GPointIndexLookup
//! @see gdraw_command_image_scale_segmented_to, kino_reel_dci_transform_create
KinoReel *kino_reel_unfold_create(KinoReel *from_reel, bool take_ownership, GRect screen_frame,
                                  int32_t angle, int num_delay_groups, Fixed_S32_16 group_delay);

//! Set the animation to start as a dot. Requires the from frame to be set before use.
void kino_reel_unfold_set_start_as_dot(KinoReel *reel, int16_t radius);
