/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

/**
 * @defgroup services_compositor_default_compositor_shutter_transitions Shutter transitions
 * @ingroup services_compositor_default
 * @brief App transitions where the old app is pushed out behind a colored shutter.
 * @{
 */

/** @brief Length of the first part, moving the old app off the screen, in milliseconds. */
#define SHUTTER_TRANSITION_FIRST_DURATION_MS (2 * ANIMATION_TARGET_FRAME_INTERVAL_MS)
/** @brief Length of the second part, moving the new app in, in milliseconds. */
#define SHUTTER_TRANSITION_SECOND_DURATION_MS (4 * ANIMATION_TARGET_FRAME_INTERVAL_MS)
/** @brief Total length of the shutter transition, in milliseconds. */
#define SHUTTER_TRANSITION_DURATION_MS \
  (SHUTTER_TRANSITION_FIRST_DURATION_MS + SHUTTER_TRANSITION_SECOND_DURATION_MS)

/**
 * @brief Get the shutter transition between apps.
 *
 * @param direction Direction of the transition. No shutter is drawn for
 *                  @ref CompositorTransitionDirectionLeft.
 * @param color Color of the shutter.
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_shutter_transition_get(
    CompositorTransitionDirection direction, GColor color);

/** @} */
