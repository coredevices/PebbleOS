/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "countdown.h"

#include "style.h"
#include "utils.h"

#include <applib/ui/animation_interpolate.h>
#include <applib/ui/kino/kino_reel/transform.h>
#include <applib/ui/kino/kino_reel/unfold.h>
#include <applib/ui/ui.h>
#ifdef CONFIG_TOUCH
#include <applib/touch_service.h>
#endif
#include <pbl/services/i18n/i18n.h>
#include <pbl/services/light.h>
#include <pbl/util/size.h>

#include <kernel/pbl_malloc.h>
#include <resource/resource_ids.auto.h>

#define ICON_EXIT_MS      200
#define HEARTBEAT_MS      340
#define COUNT_INTERVAL_MS 1000
#define MORPH_MS          420
#define GO_MS             600

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

static GPoint prv_circle_center(WorkoutCountdownWindow *window) {
  const GRect area = workout_picker_icon_area(window->window.layer.bounds.size);
  return grect_center_point(&area);
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
      WORKOUT_PICKER_HEADER_Y - (morphing ? interpolate_int16(window->progress, 0, 60) : 0);
  const int footer_y = workout_picker_footer_y(GSize(w, h)) +
                       (morphing ? interpolate_int16(window->progress, 0, 90) : 0);
  workout_draw_picker_heading(ctx, w, header_y, i18n_get("WORKOUT", window));
  workout_draw_picker_name(
      ctx, w, footer_y,
      i18n_get(workout_utils_get_name_for_activity(window->activity_type), window));

  graphics_context_set_fill_color(ctx, workout_activity_color(window->activity_type));
  if (morphing) {
    // Collapse into the accent rule under the workout heading.
    const int diameter = 2 * radius + 1;
    const GRect rule = workout_heading_rule(GSize(w, h));
    const AnimationProgress progress = window->progress;
    GRect shape = GRect(interpolate_int16(progress, center.x - radius, rule.origin.x),
                        interpolate_int16(progress, center.y - radius, rule.origin.y),
                        interpolate_int16(progress, diameter, rule.size.w),
                        interpolate_int16(progress, diameter, rule.size.h));
    graphics_fill_round_rect(ctx, &shape, interpolate_int16(progress, radius, 1), GCornersAll);
  } else if (window->phase == CountdownPhase_Number) {
    graphics_fill_circle(ctx, center, radius + prv_heartbeat_offset(window->progress));
  }

  if (window->phase == CountdownPhase_Number && window->number == 0) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, i18n_get("GO!", window),
                       fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK),
                       GRect(center.x - radius, center.y - 21, 2 * radius, 40),
                       GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
  } else if (window->phase == CountdownPhase_Number) {
    const char number[] = {'0' + window->number, '\0'};
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(
        ctx, number,
        fonts_get_system_font(h >= 200 ? FONT_KEY_LECO_60_NUMBERS_AM_PM : FONT_KEY_LECO_42_NUMBERS),
        GRect(center.x - radius, center.y - (h >= 200 ? 40 : 29), 2 * radius, h >= 200 ? 75 : 58),
        GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
  }
  i18n_free_all(window);
}

static void prv_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  WorkoutCountdownWindow *window = window_get_user_data(layer_get_window(layer));
  kino_reel_draw(window->icon, ctx, GPointZero);
}

static void prv_start_number(WorkoutCountdownWindow *window) {
  light_enable_interaction();
  window->phase = CountdownPhase_Number;
  layer_set_hidden(&window->icon_layer, true);
  window->timer =
      app_timer_register(window->number ? COUNT_INTERVAL_MS : GO_MS, prv_timer_callback, window);
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
    kino_reel_set_elapsed(window->icon, ICON_EXIT_MS * progress / ANIMATION_NORMALIZED_MAX);
    layer_mark_dirty(&window->icon_layer);
  }
  layer_mark_dirty(&window->base_layer);
}

static void prv_animation_stopped(Animation *animation, bool finished, void *context) {
  if (!finished) {
    return;
  }
  WorkoutCountdownWindow *window = context;
  window->animation = nullptr;
  window->progress = ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(&window->base_layer);
  prv_complete_phase(window);
}

static void prv_start_animation(WorkoutCountdownWindow *window, uint32_t duration) {
  workout_cancel_animation(&window->animation);
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
  animation_set_curve(window->animation, window->phase == CountdownPhase_Bar
                                             ? AnimationCurveEaseInOut
                                             : AnimationCurveLinear);
  animation_set_handlers(window->animation, (AnimationHandlers){.stopped = prv_animation_stopped},
                         window);
  animation_schedule(window->animation);
}

static void prv_timer_callback(void *data) {
  WorkoutCountdownWindow *window = data;
  window->timer = nullptr;
  workout_cancel_animation(&window->animation);
  if (--window->number >= 0) {
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
  workout_cancel_animation(&countdown->animation);
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
  KinoReel *icon = workout_create_activity_icon(type);
  Window *window = &countdown->window;
  window_init(window, WINDOW_NAME("Workout Countdown"));
  window_set_user_data(window, countdown);
  window_set_background_color(window, GColorBlack);
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
  // Same place as on the picker, which the icon leaves by shrinking into a dot.
  const GRect icon_frame =
      workout_picker_icon_rect(window->layer.bounds.size, kino_reel_get_size(icon));
  layer_init(&countdown->icon_layer, &icon_frame);
  layer_set_clips(&countdown->icon_layer, false);
  layer_set_update_proc(&countdown->icon_layer, prv_icon_layer_update_proc);
  layer_add_child(&window->layer, &countdown->icon_layer);
  GRect from;
  layer_get_global_frame(&countdown->icon_layer, &from);
  countdown->icon = kino_reel_unfold_create(icon, true, from, 0, UNFOLD_DEFAULT_NUM_DELAY_GROUPS,
                                            UNFOLD_DEFAULT_GROUP_DELAY);
  if (countdown->icon) {
    kino_reel_transform_set_to_frame(countdown->icon, (GRect){grect_center_point(&from)});
    kino_reel_transform_set_transform_duration(countdown->icon, ICON_EXIT_MS);
    kino_reel_scale_segmented_set_end_as_dot(countdown->icon, WORKOUT_ICON_DOT_RADIUS);
  } else {
    countdown->icon = icon;
  }
  app_window_stack_push(window, false);
  prv_start_animation(countdown, ICON_EXIT_MS);
}
