/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

/**
 * @defgroup services_compositor_default_compositor_round_flip_transitions Round flip transitions
 * @ingroup services_compositor_default
 * @brief Horizontal flip transitions between apps, shaped for round displays.
 * @{
 */

/** @brief Duration of the flip transition, in milliseconds (as many frames as the design). */
#define ROUND_FLIP_ANIMATION_DURATION_MS (6 * ANIMATION_TARGET_FRAME_INTERVAL_MS)

/**
 * @brief Draw the flip "lid" for the current point of a flip animation.
 *
 * The lid sweeps across the display and flips halfway through the animation.
 *
 * @param ctx Graphics context to draw into.
 * @param distance_normalized Animation progress, 0 to @c ANIMATION_NORMALIZED_MAX.
 * @param dir Direction of the visual elements.
 * @param flip_lid_color Fill color of the lid.
 */
void compositor_round_flip_transitions_flip_animation_update(GContext *ctx,
                                                             uint32_t distance_normalized,
                                                             CompositorTransitionDirection dir,
                                                             GColor flip_lid_color);

/**
 * @brief Get the flip transition between apps.
 *
 * @param flip_to_the_right Whether the visual elements flip to the right.
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_round_flip_transition_get(bool flip_to_the_right);

/** @} */
