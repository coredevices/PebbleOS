/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "timeline_layer.h"
#include "applib/ui/animation.h"

typedef enum {
  RelationshipBarOffsetTypePrev,
  RelationshipBarOffsetTypeCurr,
  RelationshipBarOffsetTypeBoth
} RelationshipBarOffsetType;

//! Create the animations for the relationship bars
//! @param layer Pointer to the \ref TimelineLayer to create the relationship bar animation for
//! @param duration Duration in ms of the animation to create
//! @param interpolate Custom interpolation function to use for the animation
Animation *timeline_relbar_layer_create_animation(TimelineLayer *timeline_layer, uint32_t duration,
                                                  InterpolateInt64Function interpolate);

//! Reset the relationship bar state
void timeline_relbar_layer_reset(TimelineLayer *timeline_layer);

//! Initialize the timeline relationship bar layer within the \ref TimelineLayer
void timeline_relbar_layer_init(TimelineLayer *timeline_layer);

//! Deinitialize the timeline relationship bar layer within the \ref TimelineLayer
void timeline_relbar_layer_deinit(TimelineLayer *timeline_layer);
