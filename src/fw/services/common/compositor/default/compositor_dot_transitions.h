/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

// These numbers approximate the visuals shown in the videos from the design team
#define STATIC_DOT_ANIMATION_DURATION_MS 233
#define DOT_ANIMATION_STROKE_WIDTH 12

void compositor_dot_transitions_collapsing_ring_animation_update(GContext *ctx,
                                                                 uint32_t distance_normalized,
                                                                 GColor outer_ring_color,
                                                                 GColor inner_ring_color);

const CompositorTransition* compositor_dot_transition_timeline_get(bool timeline_is_future,
                                                                   bool timeline_is_destination);

const CompositorTransition* compositor_dot_transition_app_fetch_get(void);
