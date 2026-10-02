/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/compositor/compositor.h"

/**
 * @defgroup services_compositor_default Default transitions
 * @ingroup services_compositor
 * @brief Compositor transitions of the current design.
 *
 * The modal, port hole and round flip transitions are only built for color displays; 1-bit
 * displays use @ref services_compositor_legacy for modals instead.
 */

/**
 * @defgroup services_compositor_default_compositor_dot_transitions Dot transitions
 * @ingroup services_compositor_default
 * @brief Collapse the screen to a dot, or expand a dot into the new screen.
 * @{
 */

/**
 * @brief Duration of the static dot transitions, in milliseconds.
 *
 * Approximates the visuals of the design team's videos.
 */
#define STATIC_DOT_ANIMATION_DURATION_MS 233
/** @brief Stroke width of the dot ring, in pixels. */
#define DOT_ANIMATION_STROKE_WIDTH 12

/**
 * @brief Draw a ring collapsing from the display edges to a dot at its center.
 *
 * @param ctx Graphics context to draw into; the ring is centered in its clip box.
 * @param distance_normalized Animation progress, 0 to @c ANIMATION_NORMALIZED_MAX.
 * @param outer_ring_color Color of the collapsing outer ring.
 * @param inner_ring_color Color of the thin ring along its inner edge.
 */
void compositor_dot_transitions_collapsing_ring_animation_update(GContext *ctx,
                                                                 uint32_t distance_normalized,
                                                                 GColor outer_ring_color,
                                                                 GColor inner_ring_color);

/**
 * @brief Get the dot transition into or out of the timeline.
 *
 * @param timeline_is_future Whether the timeline shows the future (otherwise the past).
 * @param timeline_is_destination Whether the transition goes to the timeline.
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_dot_transition_timeline_get(bool timeline_is_future,
                                                                   bool timeline_is_destination);

/**
 * @brief Get the dot transition shown when leaving the app fetch screen.
 *
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_dot_transition_app_fetch_get(void);

/** @} */
