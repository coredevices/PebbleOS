/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

/**
 * @defgroup services_compositor_default_compositor_modal_transitions Modal transitions
 * @ingroup services_compositor_default
 * @brief Transitions pushing and popping modal windows on color displays.
 * @{
 */

/**
 * @brief Get the transition to or from a modal window.
 *
 * Contracts to or expands from a ring, using PDC sequences on rectangular displays (none in
 * PRF).
 *
 * @param modal_is_destination Whether the transition goes to the modal.
 * @return Transition.
 */
const CompositorTransition *compositor_modal_transition_to_modal_get(bool modal_is_destination);

/** @} */
