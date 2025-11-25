/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

#define PORT_HOLE_TRANSITION_DURATION_MS (6 * ANIMATION_TARGET_FRAME_INTERVAL_MS)

const CompositorTransition *compositor_port_hole_transition_app_get(
  CompositorTransitionDirection direction);

void compositor_port_hole_transition_draw_outer_ring(GContext *ctx, int16_t pixels,
                                                     GColor ring_color);
