/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "window_stack_animation_rect.h"

#include "window_private.h"
#include "window_stack.h"

#include "applib/graphics/graphics_private.h"
#include "applib/legacy2/ui/property_animation_legacy2.h"

static void prv_update_rect_compatible(Animation *a, const AnimationProgress progress) {
  bool uses_legacy2_animations = animation_private_using_legacy_2(NULL);
  if (uses_legacy2_animations) {
    property_animation_legacy2_update_grect((PropertyAnimationLegacy2 *)a, progress);
  } else {
    property_animation_update_grect((PropertyAnimation *)a, progress);
  }
}

static void prv_window_frame_setter(void *subject, GRect rect) {
  WindowTransitioningContext *ctx = subject;
  Window *const window = ctx->window_to;
  if (!window) {
    // The window has been unloaded already, but the animation wasn't able to be unscheduled.
    return;
  }
  Layer *const root_layer = window_get_root_layer(window);

  // when transitioning a 2.x app, don't modify the window.frame for the window_to
  // but use the workaround transition_context.window_to_displacement
  if (window_transition_context_has_legacy_window_to(window->parent_window_stack, window)) {
    ctx->window_to_displacement = rect.origin;
    layer_mark_dirty(root_layer);
    return;
  }

  layer_set_frame(root_layer, &rect);
}

static void prv_transition_setup_window_callbacks(Animation *animation) {
  WindowTransitioningContext *context = animation_get_context(animation);
  window_transition_context_appearance_call_all(context);

  // make sure we don't render the to_window accidentally at a default origin
  if (animation_private_using_legacy_2(NULL)) {
    // on 2.x we don't need to consider any easing
    animation_get_implementation(animation)->update(animation, 0);
  } else {
    animation_private_update(NULL, animation_private_animation_find(animation), 0);
  }
}

static void prv_transition_teardown_destroy_animation(Animation *a) {
  // needed for compatibility with 2.x apps: manually free animation + clear pointer
  WindowTransitioningContext *ctx = animation_get_context(a);
  ctx->animation = NULL;
  animation_destroy(a);
}

static Animation *prv_window_transition_move(WindowTransitioningContext *ctx,
                                             int16_t start_delta_x) {
  static struct PropertyAnimationImplementation const impl = {
    .base = {
      .setup = prv_transition_setup_window_callbacks,
      .update = prv_update_rect_compatible,
      .teardown = prv_transition_teardown_destroy_animation,
    },
    .accessors = {
      .setter.grect = prv_window_frame_setter,
    },
  };

  Window *window = ctx->window_to;
  GRect window_to_end = window_calc_frame(window->is_fullscreen);
  GRect window_to_start = window_to_end;
  window_to_start.origin.x += start_delta_x;

  PropertyAnimation *prop_animation = property_animation_create(&impl, ctx, NULL, NULL);

  property_animation_set_from_grect(prop_animation, &window_to_start);
  property_animation_set_to_grect(prop_animation, &window_to_end);

  Animation *animation = property_animation_get_animation(prop_animation);
  animation_set_handlers(animation, (AnimationHandlers){}, ctx);

  if (!process_manager_compiled_with_legacy2_sdk()) {
    animation_set_custom_interpolation(animation, interpolate_moook);
    animation_set_duration(animation, interpolate_moook_duration());
  }

  return property_animation_get_animation(prop_animation);
}

static void prv_window_transition_move_render(WindowTransitioningContext *context, GContext *ctx) {
  Window *window_from = context->window_from;
  if (window_from) {
    window_render(window_from, ctx);
    graphics_patch_trace_of_moving_rect(ctx, &context->window_from_last_x,
                                        window_from->layer.frame);
  }

  Window *window_to = context->window_to;
  if (window_to) {
    window_render(window_to, ctx);
    graphics_patch_trace_of_moving_rect(ctx, &context->window_to_last_x, window_to->layer.frame);
  }
}

static Animation *prv_window_transition_move_from_right_create_animation(
  WindowTransitioningContext *context) {
  return prv_window_transition_move(context, DISP_COLS);
}

static Animation *prv_window_transition_move_from_left_create_animation(
  WindowTransitioningContext *context) {
  return prv_window_transition_move(context, -DISP_COLS);
}

const WindowTransitionImplementation g_window_transition_default_push_implementation_rect = {
  .create_animation = prv_window_transition_move_from_right_create_animation,
  .render = prv_window_transition_move_render,
};

const WindowTransitionImplementation g_window_transition_default_pop_implementation_rect = {
  .create_animation = prv_window_transition_move_from_left_create_animation,
  .render = prv_window_transition_move_render,
};

static void prv_update_null(Animation *animation, const AnimationProgress distance_normalized) {
  // do nothing
}

static Animation *prv_window_transition_none_create_animation(WindowTransitioningContext *context) {
  static struct AnimationImplementation const impl = {
    .setup = prv_transition_setup_window_callbacks,
    .update = prv_update_null,
    .teardown = prv_transition_teardown_destroy_animation,
  };
  Animation *result = animation_create();
  animation_set_handlers(result, (AnimationHandlers){}, context);
  animation_set_implementation(result, &impl);
  animation_set_duration(result, 0);
  return result;
}

const WindowTransitionImplementation g_window_transition_none_implementation = {
  .create_animation = prv_window_transition_none_create_animation,
  .render = prv_window_transition_move_render,
};
