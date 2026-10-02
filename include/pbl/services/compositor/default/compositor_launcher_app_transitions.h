/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/compositor/compositor.h"

#include "apps/system/launcher/launcher.h"

/**
 * @defgroup services_compositor_default_compositor_launcher_app_transitions Launcher transitions
 * @ingroup services_compositor_default
 * @brief Transitions between the launcher and apps, moving the launcher's selected row.
 * @{
 */

/**
 * @brief Get the transition between the launcher and an app.
 *
 * Uses the draw state the launcher saved before closing.
 *
 * @param app_is_destination Whether the transition goes from the launcher to the app.
 * @return Transition, or NULL if app-to-app transitions should be skipped.
 */
const CompositorTransition *compositor_launcher_app_transition_get(bool app_is_destination);

/** @} */
