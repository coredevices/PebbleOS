/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

//! @file compositor_modal_transitions.h
//! Allows a user to create and configure compositor transition animations for modals.

//! @param modal_is_destination Whether the animation should animate to the modal or not
//! @return \ref CompositorTransition for the requested modal animation
const CompositorTransition* compositor_modal_transition_to_modal_get(bool modal_is_destination);
