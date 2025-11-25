/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "window_stack_animation.h"

#include "services/common/compositor/compositor.h"

typedef struct {
  WindowTransitionImplementation implementation;
  CompositorTransitionDirection transition_direction;
} WindowTransitionRoundImplementation;

extern const WindowTransitionRoundImplementation
  g_window_transition_default_push_implementation_round;
extern const WindowTransitionRoundImplementation
  g_window_transition_default_pop_implementation_round;
