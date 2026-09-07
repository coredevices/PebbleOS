/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "summary.h"

#include "countdown.h"
#include "style.h"
#include "utils.h"

#include <applib/ui/kino/kino_reel.h>
#include <applib/ui/kino/kino_reel/transform.h>
#include <applib/ui/kino/kino_reel/unfold.h>
#include <applib/ui/ui.h>
#include <kernel/pbl_malloc.h>
#ifdef CONFIG_TOUCH
#include <applib/touch_service.h>
#endif
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/testing.h>

#include <resource/resource_ids.auto.h>

#define BACKGROUND_COLOR GColorBlack
#define COLLAPSE_MS      200
#define UNFOLD_MS        300

typedef struct WorkoutSummaryWindow {
  Window window;
#ifdef CONFIG_TOUCH
  GPoint touch_start;
  bool touch_active;
#endif
  Layer base_layer;
  Layer icon_layer;
  Animation *icon_animation;
  KinoReel *collapse;
  KinoReel *unfold;
  bool unfolding;
  ActivitySessionType shown_type;
  const char *shown_name;

  ActivitySessionType activity_type;

  KinoReel *icon;
  const char *name;

  StartWorkoutCallback start_workout_cb;
  SelectWorkoutCallback select_workout_cb;
} WorkoutSummaryWindow;

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Helpers

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Drawing

static void prv_render_activity_type(GContext *ctx, Layer *layer, const char *name,
                                     ActivitySessionType activity_type) {
  const int w = layer->bounds.size.w;
  const int footer_y = workout_picker_footer_y(layer->bounds.size);
  workout_draw_picker_heading(ctx, w, WORKOUT_PICKER_HEADER_Y, i18n_get("WORKOUT", layer));
  workout_draw_picker_name(ctx, w, footer_y, i18n_get(name, layer));
  const ActivitySessionType types[] = {
    ActivitySessionType_Run, ActivitySessionType_Walk, ActivitySessionType_Open
  };
  for (int i = 0; i < 3; i++) {
    const GColor color = workout_activity_color(types[i]);
    const GPoint center = GPoint(w / 2 - 16 + i * 16, footer_y + 46);
    graphics_context_set_fill_color(ctx, activity_type == types[i] ? color : GColorBlack);
    graphics_fill_circle(ctx, center, 4);
    graphics_context_set_stroke_color(ctx, color);
    graphics_draw_circle(ctx, center, 4);
  }
  workout_draw_select_indicator(ctx, layer->bounds.size);
  i18n_free_all(layer);
}

static void prv_base_layer_update_proc(struct Layer *layer, GContext *ctx) {
  WorkoutSummaryWindow *summary_window = window_get_user_data(layer_get_window(layer));

  prv_render_activity_type(ctx, layer, summary_window->shown_name, summary_window->shown_type);
}

static GRect prv_icon_rect(WorkoutSummaryWindow *window, KinoReel *reel) {
  return workout_picker_icon_rect(window->window.layer.bounds.size, kino_reel_get_size(reel));
}

// The unfold reels expect their layer to sit exactly on the icon they draw.
static void prv_fit_icon_layer(WorkoutSummaryWindow *window, KinoReel *reel) {
  GRect rect = prv_icon_rect(window, reel);
  layer_set_frame(&window->icon_layer, &rect);
}

static void prv_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  WorkoutSummaryWindow *window = window_get_user_data(layer_get_window(layer));
  KinoReel *reel =
      window->collapse ? (window->unfolding ? window->unfold : window->collapse) : window->icon;
  kino_reel_draw(reel, ctx, GPointZero);
}

static void prv_show_type(WorkoutSummaryWindow *window) {
  window->shown_type = window->activity_type;
  window->shown_name = window->name;
  layer_mark_dirty(&window->base_layer);
}

static void prv_stop_icon_animation(WorkoutSummaryWindow *window) {
  workout_cancel_animation(&window->icon_animation);
  kino_reel_destroy(window->collapse);
  window->collapse = nullptr;
  kino_reel_destroy(window->unfold);
  window->unfold = nullptr;
  window->unfolding = false;
  if (window->icon) {
    prv_fit_icon_layer(window, window->icon);
  }
  prv_show_type(window);
  layer_mark_dirty(&window->icon_layer);
}

static void prv_icon_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutSummaryWindow *window = animation_get_context(animation);
  const uint32_t elapsed = (COLLAPSE_MS + UNFOLD_MS) * progress / ANIMATION_NORMALIZED_MAX;
  if (elapsed < COLLAPSE_MS) {
    kino_reel_set_elapsed(window->collapse, elapsed);
  } else {
    if (!window->unfolding) {
      // Out of the dot comes the new type.
      window->unfolding = true;
      prv_fit_icon_layer(window, window->icon);
      prv_show_type(window);
    }
    kino_reel_set_elapsed(window->unfold, elapsed - COLLAPSE_MS);
  }
  layer_mark_dirty(&window->icon_layer);
}

static void prv_icon_animation_stopped(Animation *animation, bool finished, void *context) {
  if (!finished) {
    return;
  }
  WorkoutSummaryWindow *window = context;
  window->icon_animation = nullptr;
  prv_stop_icon_animation(window);
}

static void prv_animate_icon(WorkoutSummaryWindow *window, KinoReel *outgoing) {
  Layer *layer = &window->icon_layer;
  GRect to;
  prv_fit_icon_layer(window, window->icon);
  layer_get_global_frame(layer, &to);
  GRect from;
  prv_fit_icon_layer(window, outgoing);
  layer_get_global_frame(layer, &from);
  const GRect dot = {grect_center_point(&from), GSizeZero};

  window->collapse = kino_reel_unfold_create(
      outgoing, true, from, 0, UNFOLD_DEFAULT_NUM_DELAY_GROUPS, UNFOLD_DEFAULT_GROUP_DELAY);
  window->unfold = kino_reel_unfold_create(
      window->icon, false, to, 0, UNFOLD_DEFAULT_NUM_DELAY_GROUPS, UNFOLD_DEFAULT_GROUP_DELAY);
  window->icon_animation = animation_create();
  if (!window->collapse || !window->unfold || !window->icon_animation) {
    if (!window->collapse) {
      kino_reel_destroy(outgoing);
    }
    prv_stop_icon_animation(window);
    return;
  }
  kino_reel_transform_set_to_frame(window->collapse, dot);
  kino_reel_transform_set_transform_duration(window->collapse, COLLAPSE_MS);
  kino_reel_scale_segmented_set_end_as_dot(window->collapse, WORKOUT_ICON_DOT_RADIUS);
  kino_reel_transform_set_from_frame(window->unfold, dot);
  kino_reel_transform_set_to_frame(window->unfold, to);
  kino_reel_transform_set_transform_duration(window->unfold, UNFOLD_MS);
  kino_reel_unfold_set_start_as_dot(window->unfold, WORKOUT_ICON_DOT_RADIUS);
  kino_reel_scale_segmented_set_deflate_effect(window->unfold, 5);
  kino_reel_scale_segmented_set_bounce_effect(window->unfold, 4);

  static const AnimationImplementation implementation = {.update = prv_icon_animation_update};
  animation_set_implementation(window->icon_animation, &implementation);
  animation_set_handlers(window->icon_animation,
                         (AnimationHandlers){.stopped = prv_icon_animation_stopped}, window);
  animation_set_duration(window->icon_animation, COLLAPSE_MS + UNFOLD_MS);
  animation_set_curve(window->icon_animation, AnimationCurveLinear);
  animation_schedule(window->icon_animation);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Handlers

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutSummaryWindow *summary_window = context;
  prv_stop_icon_animation(summary_window);
  workout_countdown_start(summary_window->activity_type, summary_window->start_workout_cb);
}

PBL_T_STATIC void prv_cycle_activity(WorkoutSummaryWindow *summary_window, int direction) {
  const ActivitySessionType types[] = {
    ActivitySessionType_Run, ActivitySessionType_Walk, ActivitySessionType_Open
  };
  int index = 0;
  while (index < 2 && types[index] != summary_window->activity_type) {
    index++;
  }
  ActivitySessionType type = types[(index + direction + 3) % 3];
  const ActivitySessionType old_type = summary_window->activity_type;
  const char *old_name = summary_window->name;
  KinoReel *outgoing = summary_window->icon;
  summary_window->icon = nullptr;
  workout_summary_update_activity_type(summary_window, type);
  // The name and dots follow the icon once it has shrunk into a dot.
  summary_window->shown_type = old_type;
  summary_window->shown_name = old_name;
  prv_animate_icon(summary_window, outgoing);
  summary_window->select_workout_cb(type);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_cycle_activity(context, -1);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  prv_cycle_activity(context, 1);
}

static void prv_click_config_provider(void *context) {
  window_set_click_context(BUTTON_ID_UP, context);
  window_set_click_context(BUTTON_ID_SELECT, context);
  window_set_click_context(BUTTON_ID_DOWN, context);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

#ifdef CONFIG_TOUCH
static void prv_touch_handler(const TouchEvent *event, void *context) {
  WorkoutSummaryWindow *summary_window = context;
  if (event->type == TouchEvent_Touchdown) {
    summary_window->touch_start = GPoint(event->x, event->y);
    summary_window->touch_active = !event->non_navigational;
    return;
  }
  if (event->type != TouchEvent_Liftoff || !summary_window->touch_active) {
    return;
  }
  summary_window->touch_active = false;
  const int dx = event->x - summary_window->touch_start.x;
  const int dy = event->y - summary_window->touch_start.y;
  if (ABS(dy) > 20 && ABS(dy) > ABS(dx)) {
    prv_cycle_activity(summary_window, dy < 0 ? 1 : -1);
  } else if (ABS(dx) > 20 && ABS(dx) > ABS(dy)) {
    prv_cycle_activity(summary_window, dx < 0 ? 1 : -1);
  } else if (ABS(dx) <= 20 && ABS(dy) <= 20) {
    prv_select_click_handler(nullptr, summary_window);
  }
}

static void prv_window_appear_handler(Window *window) {
  WorkoutSummaryWindow *summary_window = window_get_user_data(window);
  summary_window->touch_active = false;
  touch_service_subscribe(prv_touch_handler, summary_window);
}

static void prv_window_disappear_handler(Window *window) {
  touch_service_unsubscribe();
}
#endif

static void prv_window_unload_handler(Window *window) {
  WorkoutSummaryWindow *summary_window = window_get_user_data(window);
  if (summary_window) {
    prv_stop_icon_animation(summary_window);
    kino_reel_destroy(summary_window->icon);
    layer_deinit(&summary_window->icon_layer);
    layer_deinit(&summary_window->base_layer);
    window_deinit(&summary_window->window);
    i18n_free_all(summary_window);
    app_free(summary_window);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Public API

WorkoutSummaryWindow *workout_summary_window_create(ActivitySessionType activity_type,
                                                    StartWorkoutCallback start_workout_cb,
                                                    SelectWorkoutCallback select_workout_cb) {
  WorkoutSummaryWindow *summary_window = app_zalloc_check(sizeof(WorkoutSummaryWindow));

  summary_window->start_workout_cb = start_workout_cb;
  summary_window->select_workout_cb = select_workout_cb;

  Window *window = &summary_window->window;
  window_init(window, WINDOW_NAME("Choose Workout"));
  window_set_user_data(window, summary_window);
  window_set_background_color(window, BACKGROUND_COLOR);
  window_set_window_handlers(window, &(WindowHandlers){
                                       .unload = prv_window_unload_handler,
#ifdef CONFIG_TOUCH
                                       .appear = prv_window_appear_handler,
                                       .disappear = prv_window_disappear_handler,
#endif
                                     });

  GRect layer_bounds = window->layer.bounds;

  layer_init(&summary_window->base_layer, &layer_bounds);
  layer_set_update_proc(&summary_window->base_layer, prv_base_layer_update_proc);
  layer_add_child(&window->layer, &summary_window->base_layer);

  GRect icon_bounds = workout_picker_icon_area(layer_bounds.size);
  layer_init(&summary_window->icon_layer, &icon_bounds);
  layer_set_clips(&summary_window->icon_layer, false);
  layer_set_update_proc(&summary_window->icon_layer, prv_icon_layer_update_proc);
  layer_add_child(&window->layer, &summary_window->icon_layer);
  window_set_click_config_provider_with_context(window, prv_click_config_provider, summary_window);

  workout_summary_update_activity_type(summary_window, activity_type);

  return summary_window;
}

void workout_summary_window_push(WorkoutSummaryWindow *summary_window) {
  app_window_stack_push(&summary_window->window, true);
}

void workout_summary_update_activity_type(WorkoutSummaryWindow *summary_window,
                                          ActivitySessionType activity_type) {
  prv_stop_icon_animation(summary_window);
  summary_window->activity_type = activity_type;
  kino_reel_destroy(summary_window->icon);
  summary_window->icon = workout_create_activity_icon(activity_type);
  summary_window->name = workout_utils_get_name_for_activity(activity_type);
  prv_show_type(summary_window);
  prv_fit_icon_layer(summary_window, summary_window->icon);
  layer_mark_dirty(&summary_window->base_layer);
  layer_mark_dirty(&summary_window->icon_layer);
}

void workout_summary_window_remove(WorkoutSummaryWindow *summary_window) {
  window_stack_remove(&summary_window->window, false);
}
