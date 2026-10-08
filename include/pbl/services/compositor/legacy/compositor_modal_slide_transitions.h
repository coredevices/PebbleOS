/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

/**
 * @defgroup services_compositor_legacy_compositor_modal_slide_transitions Modal slides
 * @ingroup services_compositor_legacy
 * @brief Vertical slides pushing and popping modal windows on 1-bit displays.
 *
 * Implements compositor_modal_transition_to_modal_get() on 1-bit displays, in place of the color
 * version: the modal slides up from the bottom when pushed and down when popped.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
const CompositorTransition *compositor_modal_transition_to_modal_get(bool modal_is_destination);
/** @endcond */

/** @} */
