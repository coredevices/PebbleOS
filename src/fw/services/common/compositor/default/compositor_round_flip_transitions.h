/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

// Animation in design video lasts this many frames
#define ROUND_FLIP_ANIMATION_DURATION_MS (6 * ANIMATION_TARGET_FRAME_INTERVAL_MS)

void compositor_round_flip_transitions_flip_animation_update(GContext *ctx,
                                                             uint32_t distance_normalized,
                                                             CompositorTransitionDirection dir,
                                                             GColor flip_lid_color);

const CompositorTransition *compositor_round_flip_transition_get(bool flip_to_the_right);
