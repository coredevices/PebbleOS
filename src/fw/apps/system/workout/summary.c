/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "summary.h"

#include "countdown.h"
#include "style.h"
#include "utils.h"

#include "applib/ui/kino/kino_layer.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#ifdef CONFIG_TOUCH
#include "applib/touch_service.h"
#endif
#include "resource/resource_ids.auto.h"
#include "pbl/services/i18n/i18n.h"

#define BACKGROUND_COLOR GColorWhite

typedef struct WorkoutSummaryWindow {
  Window window;
#ifdef CONFIG_TOUCH
  GPoint touch_start;
  bool touch_active;
#endif
  Layer base_layer;
  Layer icon_layer;
  Animation *icon_animation;
  KinoReel *outgoing_icon;
  GColor outgoing_color;
  AnimationProgress icon_progress;
  int icon_direction;

  ActivitySessionType activity_type;

  GColor icon_color;
  KinoReel *icon;
  const char *name;

  StartWorkoutCallback start_workout_cb;
  SelectWorkoutCallback select_workout_cb;
} WorkoutSummaryWindow;

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Helpers

static KinoReel *prv_get_icon_for_activity(ActivitySessionType type) {
  switch (type) {
    case ActivitySessionType_Open:
      return kino_reel_create_with_resource(RESOURCE_ID_WORKOUT_APP_WORKOUT);
    case ActivitySessionType_Walk:
      return kino_reel_create_with_resource(RESOURCE_ID_WORKOUT_APP_WALK);
    case ActivitySessionType_Run:
    default:
      return kino_reel_create_with_resource(RESOURCE_ID_WORKOUT_APP_RUN);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Drawing

static void prv_render_activity_type(GContext *ctx, Layer *layer, const char *name,
                                     ActivitySessionType activity_type) {
  const int w = layer->bounds.size.w;
  const int h = layer->bounds.size.h;
  const int header_y = PBL_IF_ROUND_ELSE(18, -2);
  const int header_h = 26;
  const int footer_y = h - PBL_IF_ROUND_ELSE(72, 57);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get("WORKOUT", layer),
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, header_y, w, header_h), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  const int side = PBL_IF_ROUND_ELSE(24, 0);
  graphics_draw_line(ctx, GPoint(side, header_y + header_h), GPoint(w - side, header_y + header_h));

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get(name, layer), fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(0, footer_y + 1, w, 34), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  const ActivitySessionType types[] = {ActivitySessionType_Run, ActivitySessionType_Walk,
                                       ActivitySessionType_Open};
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < 3; i++) {
    const GColor color = PBL_IF_COLOR_ELSE(workout_activity_color(types[i]), GColorBlack);
    GRect marker = GRect(w / 2 - 20 + i * 16, footer_y + 42, 9, 9);
    graphics_context_set_fill_color(ctx, activity_type == types[i] ? color : GColorWhite);
    graphics_fill_rect(ctx, &marker);
    graphics_context_set_stroke_color(ctx, color);
    graphics_draw_rect(ctx, &marker);
  }
  workout_draw_select_indicator(ctx, layer->bounds.size);
  i18n_free_all(layer);
}

static void prv_base_layer_update_proc(struct Layer *layer, GContext *ctx) {
  WorkoutSummaryWindow *summary_window = window_get_user_data(layer_get_window(layer));

  prv_render_activity_type(ctx, layer, summary_window->name, summary_window->activity_type);
}

static void prv_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  WorkoutSummaryWindow *window = window_get_user_data(layer_get_window(layer));
  const GSize size = layer->bounds.size;
  const int offset =
      window->icon_animation ? size.w * window->icon_progress / ANIMATION_NORMALIZED_MAX : size.w;
  const int radius = window->window.layer.bounds.size.h >= 200 ? 48 : 34;
  if (window->outgoing_icon) {
    graphics_context_set_fill_color(ctx, window->outgoing_color);
    graphics_fill_circle(ctx, GPoint(size.w / 2 - window->icon_direction * offset, size.h / 2),
                         radius);
    const GSize icon_size = kino_reel_get_size(window->outgoing_icon);
    kino_reel_draw(window->outgoing_icon, ctx,
                   GPoint((size.w - icon_size.w) / 2 - window->icon_direction * offset,
                          (size.h - icon_size.h) / 2));
  }
  graphics_context_set_fill_color(ctx, window->icon_color);
  graphics_fill_circle(
      ctx, GPoint(size.w / 2 + window->icon_direction * (size.w - offset), size.h / 2), radius);
  const GSize icon_size = kino_reel_get_size(window->icon);
  kino_reel_draw(window->icon, ctx,
                 GPoint((size.w - icon_size.w) / 2 + window->icon_direction * (size.w - offset),
                        (size.h - icon_size.h) / 2));
}

static void prv_stop_icon_animation(WorkoutSummaryWindow *window) {
  if (window->icon_animation) {
    Animation *animation = window->icon_animation;
    window->icon_animation = NULL;
    animation_unschedule(animation);
    animation_destroy(animation);
  }
  kino_reel_destroy(window->outgoing_icon);
  window->outgoing_icon = NULL;
  layer_mark_dirty(&window->icon_layer);
}

static void prv_icon_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutSummaryWindow *window = animation_get_context(animation);
  window->icon_progress = progress;
  layer_mark_dirty(&window->icon_layer);
}

static void prv_icon_animation_stopped(Animation *animation, bool finished, void *context) {
  if (!finished) {
    return;
  }
  WorkoutSummaryWindow *window = context;
  window->icon_animation = NULL;
  kino_reel_destroy(window->outgoing_icon);
  window->outgoing_icon = NULL;
  animation_destroy(animation);
  layer_mark_dirty(&window->icon_layer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//! Handlers

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  WorkoutSummaryWindow *summary_window = context;
  prv_stop_icon_animation(summary_window);
  workout_countdown_start(summary_window->activity_type, summary_window->start_workout_cb);
}

T_STATIC void prv_cycle_activity(WorkoutSummaryWindow *summary_window, int direction) {
  const ActivitySessionType types[] = {ActivitySessionType_Run, ActivitySessionType_Walk,
                                       ActivitySessionType_Open};
  int index = 0;
  while (index < 2 && types[index] != summary_window->activity_type) {
    index++;
  }
  ActivitySessionType type = types[(index + direction + 3) % 3];
  prv_stop_icon_animation(summary_window);
  summary_window->outgoing_icon = summary_window->icon;
  summary_window->outgoing_color = summary_window->icon_color;
  summary_window->icon = NULL;
  workout_summary_update_activity_type(summary_window, type);
  summary_window->icon_direction = direction;
  summary_window->icon_progress = 0;
  summary_window->icon_animation = animation_create();
  if (summary_window->icon_animation) {
    static const AnimationImplementation implementation = {.update = prv_icon_animation_update};
    animation_set_implementation(summary_window->icon_animation, &implementation);
    animation_set_handlers(summary_window->icon_animation,
                           (AnimationHandlers){.stopped = prv_icon_animation_stopped},
                           summary_window);
    animation_set_duration(summary_window->icon_animation, 240);
    animation_set_curve(summary_window->icon_animation, AnimationCurveEaseInOut);
    animation_schedule(summary_window->icon_animation);
  } else {
    prv_stop_icon_animation(summary_window);
  }
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
    prv_select_click_handler(NULL, summary_window);
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

  const int h = layer_bounds.size.h;
  const int header_bottom = PBL_IF_ROUND_ELSE(18, -2) + 26;
  const int footer_y = h - PBL_IF_ROUND_ELSE(72, 57);
  GRect icon_bounds = GRect(0, header_bottom, layer_bounds.size.w, footer_y - header_bottom);
  layer_init(&summary_window->icon_layer, &icon_bounds);
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
  summary_window->activity_type = activity_type;
  summary_window->icon_color = workout_activity_color(activity_type);
  kino_reel_destroy(summary_window->icon);
  summary_window->icon = prv_get_icon_for_activity(activity_type);
  summary_window->name = workout_utils_get_name_for_activity(activity_type);
  layer_mark_dirty(&summary_window->base_layer);
  layer_mark_dirty(&summary_window->icon_layer);
}

void workout_summary_window_remove(WorkoutSummaryWindow *summary_window) {
  window_stack_remove(&summary_window->window, false);
}
