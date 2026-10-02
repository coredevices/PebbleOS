/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/compositor/compositor.h"

/**
 * @defgroup services_compositor_default_compositor_port_hole_transitions Port hole transitions
 * @ingroup services_compositor_default
 * @brief App transitions through a ring closing in from and opening to the display edge.
 * @{
 */

/** @brief Duration of the port hole transition, in milliseconds. */
#define PORT_HOLE_TRANSITION_DURATION_MS (6 * ANIMATION_TARGET_FRAME_INTERVAL_MS)

/**
 * @brief Get the port hole transition between apps.
 *
 * @param direction Direction the content moves in.
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_port_hole_transition_app_get(
    CompositorTransitionDirection direction);

/**
 * @brief Fill a ring along the edge of the framebuffer.
 *
 * @param ctx Graphics context to draw into.
 * @param pixels Thickness of the ring, in pixels.
 * @param ring_color Color of the ring.
 */
void compositor_port_hole_transition_draw_outer_ring(GContext *ctx, int16_t pixels,
                                                     GColor ring_color);

/** @} */
