/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "countdown.h"
#include "style.h"
#include "utils.h"

#include "applib/ui/kino/kino_layer.h"
#include "applib/ui/ui.h"
#ifdef CONFIG_TOUCH
#include "applib/touch_service.h"
#endif
#include "kernel/pbl_malloc.h"
#include "pbl/services/i18n/i18n.h"
#include "resource/resource_ids.auto.h"
#include "pbl/util/size.h"
#include "pbl/services/light.h"

#define ICON_EXIT_MS 220
#define HEARTBEAT_MS 340
#define COUNT_INTERVAL_MS 1000
#define MORPH_MS 420

typedef enum {
  CountdownPhase_Icon,
  CountdownPhase_Number,
  CountdownPhase_Bar,
} CountdownPhase;

typedef struct WorkoutCountdownWindow {
  Window window;
  Layer base_layer;
  Layer icon_layer;
  KinoReel *icon;
  Animation *animation;
  AnimationProgress progress;
  CountdownPhase phase;
  int number;
  AppTimer *timer;
#ifdef CONFIG_TOUCH
  GPoint touch_start;
  bool touch_active;
#endif
  StartWorkoutCallback start_workout_cb;
  ActivitySessionType activity_type;
} WorkoutCountdownWindow;

static void prv_timer_callback(void *data);
static void prv_start_animation(WorkoutCountdownWindow *window, uint32_t duration);

static int prv_interpolate(int from, int to, AnimationProgress progress) {
  return from + (to - from) * progress / ANIMATION_NORMALIZED_MAX;
}

static GPoint prv_circle_center(WorkoutCountdownWindow *window) {
  const GSize size = window->window.layer.bounds.size;
  const int header_bottom = PBL_IF_ROUND_ELSE(18, -2) + 26;
  const int footer_y = size.h - PBL_IF_ROUND_ELSE(72, 57);
  return GPoint(size.w / 2, (header_bottom + footer_y) / 2);
}

static int prv_heartbeat_offset(AnimationProgress progress) {
  const int times[] = {0, 70, 150, 220, HEARTBEAT_MS};
  const int offsets[] = {0, 6, 0, 3, 0};
  const int elapsed = progress * HEARTBEAT_MS / ANIMATION_NORMALIZED_MAX;
  for (size_t i = 1; i < ARRAY_LENGTH(times); i++) {
    if (elapsed <= times[i]) {
      return offsets[i - 1] +
             (offsets[i] - offsets[i - 1]) * (elapsed - times[i - 1]) / (times[i] - times[i - 1]);
    }
  }
  return 0;
}

static void prv_base_layer_update_proc(Layer *layer, GContext *ctx) {
  WorkoutCountdownWindow *window = window_get_user_data(layer_get_window(layer));
  const int w = layer->bounds.size.w;
  const int h = layer->bounds.size.h;
  const GPoint center = prv_circle_center(window);
  const int radius = h >= 200 ? 48 : 34;
  const bool morphing = window->phase == CountdownPhase_Bar;
  const int header_y =
      PBL_IF_ROUND_ELSE(18, -2) - (morphing ? prv_interpolate(0, 60, window->progress) : 0);
  const int footer_y =
      h - PBL_IF_ROUND_ELSE(72, 57) + (morphing ? prv_interpolate(0, 90, window->progress) : 0);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, i18n_get("WORKOUT", window),
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(0, header_y, w, 26),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  const int side = PBL_IF_ROUND_ELSE(24, 0);
  graphics_draw_line(ctx, GPoint(side, header_y + 26), GPoint(w - side, header_y + 26));
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx,
                     i18n_get(workout_utils_get_name_for_activity(window->activity_type), window),
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GRect(0, footer_y + 1, w, 34),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  graphics_context_set_fill_color(
      ctx, PBL_IF_COLOR_ELSE(workout_activity_color(window->activity_type), GColorBlack));
  if (morphing) {
    const int diameter = 2 * radius + 1;
    GRect shape = GRect(prv_interpolate(center.x - radius, 0, window->progress),
                        prv_interpolate(center.y - radius, 0, window->progress),
                        prv_interpolate(diameter, w, window->progress),
                        prv_interpolate(diameter, workout_header_height(), window->progress));
    graphics_fill_round_rect(ctx, &shape, prv_interpolate(radius, 0, window->progress),
                             GCornersAll);
  } else {
    const int beat =
        window->phase == CountdownPhase_Number ? prv_heartbeat_offset(window->progress) : 0;
    graphics_fill_circle(ctx, center, radius + beat);
  }

  if (window->phase == CountdownPhase_Number) {
    const char number[] = {'0' + window->number, '\0'};
    graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
    graphics_draw_text(
        ctx, number,
        fonts_get_system_font(h >= 200 ? FONT_KEY_LECO_60_NUMBERS_AM_PM : FONT_KEY_LECO_42_NUMBERS),
        GRect(center.x - radius, center.y - (h >= 200 ? 40 : 29), 2 * radius, h >= 200 ? 75 : 58),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
  i18n_free_all(window);
}

static void prv_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  WorkoutCountdownWindow *window = window_get_user_data(layer_get_window(layer));
  const GSize size = kino_reel_get_size(window->icon);
  kino_reel_draw(window->icon, ctx, GPoint(0, -(size.h - layer->bounds.size.h) / 2));
}

static void prv_cancel_animation(WorkoutCountdownWindow *window) {
  if (window->animation) {
    Animation *animation = window->animation;
    window->animation = NULL;
    animation_unschedule(animation);
    animation_destroy(animation);
  }
}

static void prv_start_number(WorkoutCountdownWindow *window) {
  light_enable_interaction();
  window->phase = CountdownPhase_Number;
  layer_set_hidden(&window->icon_layer, true);
  window->timer = app_timer_register(COUNT_INTERVAL_MS, prv_timer_callback, window);
  if (!window->timer) {
    window_stack_remove(&window->window, false);
    return;
  }
  prv_start_animation(window, HEARTBEAT_MS);
}

static void prv_complete_phase(WorkoutCountdownWindow *window) {
  if (window->phase == CountdownPhase_Icon) {
    prv_start_number(window);
  } else if (window->phase == CountdownPhase_Bar) {
    window->start_workout_cb(window->activity_type);
    window_stack_remove(&window->window, false);
    vibes_long_pulse();
  }
}

static void prv_animation_update(Animation *animation, AnimationProgress progress) {
  WorkoutCountdownWindow *window = animation_get_context(animation);
  window->progress = progress;
  if (window->phase == CountdownPhase_Icon) {
    const GSize size = kino_reel_get_size(window->icon);
    const GPoint center = prv_circle_center(window);
    const int height = prv_interpolate(size.h, 0, progress);
    layer_set_frame(&window->icon_layer,
                    &GRect(center.x - size.w / 2, center.y - height / 2, size.w, height));
    layer_set_bounds(&window->icon_layer, &GRect(0, 0, size.w, height));
  }
  layer_mark_dirty(&window->base_layer);
}

static void prv_animation_stopped(Animation *animation, bool finished, void *context) {
  if (!finished) {
    return;
  }
  WorkoutCountdownWindow *window = context;
  window->animation = NULL;
  window->progress = ANIMATION_NORMALIZED_MAX;
  animation_destroy(animation);
  layer_mark_dirty(&window->base_layer);
  prv_complete_phase(window);
}

static void prv_start_animation(WorkoutCountdownWindow *window, uint32_t duration) {
  prv_cancel_animation(window);
  window->progress = 0;
  window->animation = animation_create();
  if (!window->animation) {
    window->progress = ANIMATION_NORMALIZED_MAX;
    layer_mark_dirty(&window->base_layer);
    prv_complete_phase(window);
    return;
  }
  static const AnimationImplementation implementation = {.update = prv_animation_update};
  animation_set_implementation(window->animation, &implementation);
  animation_set_duration(window->animation, duration);
  animation_set_curve(window->animation, window->phase == CountdownPhase_Number
                                             ? AnimationCurveLinear
                                             : AnimationCurveEaseInOut);
  animation_set_handlers(window->animation, (AnimationHandlers){.stopped = prv_animation_stopped},
                         window);
  animation_schedule(window->animation);
}

static void prv_timer_callback(void *data) {
  WorkoutCountdownWindow *window = data;
  window->timer = NULL;
  prv_cancel_animation(window);
  if (--window->number > 0) {
    prv_start_number(window);
  } else {
    window->phase = CountdownPhase_Bar;
    prv_start_animation(window, MORPH_MS);
  }
}

#ifdef CONFIG_TOUCH
static void prv_touch_handler(const TouchEvent *event, void *context) {
  WorkoutCountdownWindow *window = context;
  if (event->type == TouchEvent_Touchdown) {
    window->touch_start = GPoint(event->x, event->y);
    window->touch_active = !event->non_navigational;
  } else if (event->type == TouchEvent_Liftoff && window->touch_active) {
    window->touch_active = false;
    const int dx = event->x - window->touch_start.x;
    const int dy = event->y - window->touch_start.y;
    if (dx > 30 && dx > ABS(dy)) {
      window_stack_remove(&window->window, false);
    }
  }
}

static void prv_window_appear_handler(Window *window) {
  touch_service_subscribe(prv_touch_handler, window_get_user_data(window));
}

static void prv_window_disappear_handler(Window *window) {
  touch_service_unsubscribe();
}
#endif

static void prv_window_unload_handler(Window *window) {
  WorkoutCountdownWindow *countdown = window_get_user_data(window);
  prv_cancel_animation(countdown);
  if (countdown->timer) {
    app_timer_cancel(countdown->timer);
  }
  kino_reel_destroy(countdown->icon);
  layer_deinit(&countdown->icon_layer);
  layer_deinit(&countdown->base_layer);
  window_deinit(window);
  i18n_free_all(countdown);
  app_free(countdown);
}

void workout_countdown_start(ActivitySessionType type, StartWorkoutCallback start_workout_cb) {
  WorkoutCountdownWindow *countdown = app_zalloc_check(sizeof(WorkoutCountdownWindow));
  countdown->activity_type = type;
  countdown->start_workout_cb = start_workout_cb;
  countdown->number = 3;
  countdown->icon = kino_reel_create_with_resource(
      type == ActivitySessionType_Run    ? RESOURCE_ID_WORKOUT_APP_RUN
      : type == ActivitySessionType_Walk ? RESOURCE_ID_WORKOUT_APP_WALK
                                         : RESOURCE_ID_WORKOUT_APP_WORKOUT);
  Window *window = &countdown->window;
  window_init(window, WINDOW_NAME("Workout Countdown"));
  window_set_user_data(window, countdown);
  window_set_background_color(window, GColorWhite);
  window_set_window_handlers(window, &(WindowHandlers){
                                         .unload = prv_window_unload_handler,
#ifdef CONFIG_TOUCH
                                         .appear = prv_window_appear_handler,
                                         .disappear = prv_window_disappear_handler,
#endif
                                     });
  layer_init(&countdown->base_layer, &window->layer.bounds);
  layer_set_update_proc(&countdown->base_layer, prv_base_layer_update_proc);
  layer_add_child(&window->layer, &countdown->base_layer);
  const GSize icon_size = kino_reel_get_size(countdown->icon);
  const GPoint center = prv_circle_center(countdown);
  const GRect icon_frame =
      GRect(center.x - icon_size.w / 2, center.y - icon_size.h / 2, icon_size.w, icon_size.h);
  layer_init(&countdown->icon_layer, &icon_frame);
  layer_set_update_proc(&countdown->icon_layer, prv_icon_layer_update_proc);
  layer_add_child(&window->layer, &countdown->icon_layer);
  app_window_stack_push(window, false);
  prv_start_animation(countdown, ICON_EXIT_MS);
}
