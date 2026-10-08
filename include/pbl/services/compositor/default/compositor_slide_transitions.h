/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

/**
 * @defgroup services_compositor_default_compositor_slide_transitions Slide transitions
 * @ingroup services_compositor_default
 * @brief Full-screen vertical slides, into the timeline or between apps.
 * @{
 */

/**
 * @brief Get the vertical slide into or out of the timeline.
 *
 * @param timeline_is_future Whether the timeline shows the future (otherwise the past); also
 *                           selects the fill color.
 * @param timeline_is_destination Whether the transition goes to the timeline.
 * @param timeline_is_empty Whether the timeline has no items.
 * @return Transition.
 */
const CompositorTransition *compositor_slide_transition_timeline_get(bool timeline_is_future,
                                                                     bool timeline_is_destination,
                                                                     bool timeline_is_empty);

/**
 * @brief Get the full-screen vertical push into an app.
 *
 * The incoming app framebuffer slides in, pushing the current framebuffer out.
 *
 * @param slide_up If true, the app enters from the bottom.
 * @return Transition.
 */
const CompositorTransition *compositor_slide_transition_app_get(bool slide_up);

/** @} */
