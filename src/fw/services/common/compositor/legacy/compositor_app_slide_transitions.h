/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

void compositor_app_slide_transitions_animation_update(GContext *ctx,
                                                       uint32_t distance_normalized,
                                                       CompositorTransitionDirection dir);

const CompositorTransition *compositor_app_slide_transition_get(bool flip_to_the_right);
