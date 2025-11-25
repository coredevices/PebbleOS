/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

#include "apps/system_apps/launcher/launcher_app.h"

//! @file compositor_launcher_app_transitions.h
//! Allows a user to create and configure compositor transition animations between the launcher
//! and apps.

//! @param app_is_destination Whether the animation should reflect the app as the destination
//! @return \ref CompositorTransition for the resulting animation
const CompositorTransition *compositor_launcher_app_transition_get(bool app_is_destination);
