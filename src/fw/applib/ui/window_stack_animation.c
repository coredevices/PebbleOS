/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "window_stack_animation.h"

#include "window_stack.h"
#include "window_stack_animation_rect.h"
#include "window_stack_animation_round.h"

void window_transition_context_appearance_call_all(WindowTransitioningContext *ctx) {
  window_transition_context_disappear(ctx);
  window_transition_context_appear(ctx);
}

const WindowTransitionImplementation *window_transition_get_default_push_implementation(void) {
#if PBL_RECT
  return &g_window_transition_default_push_implementation_rect;
#else
  return &g_window_transition_default_push_implementation_round.implementation;
#endif
}

const WindowTransitionImplementation *window_transition_get_default_pop_implementation(void) {
#if PBL_RECT
  return &g_window_transition_default_pop_implementation_rect;
#else
  return &g_window_transition_default_pop_implementation_round.implementation;
#endif
}
