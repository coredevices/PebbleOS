/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

// The length of first "section" of the animation, where the old app is moved off of the screen.
#define SHUTTER_TRANSITION_FIRST_DURATION_MS (2 * ANIMATION_TARGET_FRAME_INTERVAL_MS)
// The length of second "section" of the animation, where the new app is moved in.
#define SHUTTER_TRANSITION_SECOND_DURATION_MS (4 * ANIMATION_TARGET_FRAME_INTERVAL_MS)
// Total length of the animation.
#define SHUTTER_TRANSITION_DURATION_MS (SHUTTER_TRANSITION_FIRST_DURATION_MS + \
                                        SHUTTER_TRANSITION_SECOND_DURATION_MS)

const CompositorTransition *compositor_shutter_transition_get(
    CompositorTransitionDirection direction, GColor color);
