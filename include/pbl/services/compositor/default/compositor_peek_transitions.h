/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor_transitions.h>

/**
 * @defgroup services_compositor_default_compositor_peek_transitions Peek transitions
 * @ingroup services_compositor_default
 * @brief Transition from the timeline peek into the timeline.
 * @{
 */

/**
 * @brief Get the transition from the timeline peek into the timeline.
 *
 * @return Transition.
 */
const CompositorTransition *compositor_peek_transition_timeline_get(void);

/** @} */
