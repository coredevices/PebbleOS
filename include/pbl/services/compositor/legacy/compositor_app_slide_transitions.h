/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/compositor/compositor.h"

/**
 * @defgroup services_compositor_legacy Legacy transitions
 * @ingroup services_compositor
 * @brief Slide transitions used on 1-bit displays.
 */

/**
 * @defgroup services_compositor_legacy_compositor_app_slide_transitions App slide transitions
 * @ingroup services_compositor_legacy
 * @brief Horizontal slides between apps.
 * @{
 */

/**
 * @brief Draw the app framebuffer sliding in horizontally.
 *
 * Pixels uncovered past the destination (the animation overshoots) are filled with black.
 *
 * @param ctx Graphics context to draw into.
 * @param distance_normalized Animation progress, 0 to @c ANIMATION_NORMALIZED_MAX.
 * @param dir Direction of the visual elements.
 */
void compositor_app_slide_transitions_animation_update(GContext *ctx, uint32_t distance_normalized,
                                                       CompositorTransitionDirection dir);

/**
 * @brief Get the horizontal slide transition between apps.
 *
 * @param flip_to_the_right Whether the visual elements move to the right.
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_app_slide_transition_get(bool flip_to_the_right);

/** @} */
