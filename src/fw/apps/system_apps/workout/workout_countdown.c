/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "workout_countdown.h"

#include "applib/ui/ui.h"
#include "applib/ui/kino/kino_layer.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_ids.auto.h"

#define TIMER_DURATION (1000)
#define NUM_IMAGES (3)

typedef struct WorkoutCountdownWindow {
  Window window;
  Layer base_layer;

  KinoReel *images[NUM_IMAGES];
  int current_image;

  StartWorkoutCallback start_workout_cb;
  ActivitySessionType activity_type;

  AppTimer *timer;
} WorkoutCountdownWindow;

static void prv_base_layer_update_proc(Layer *layer, GContext *ctx) {
  WorkoutCountdownWindow *countdown_window = window_get_user_data(layer_get_window(layer));

  KinoReel *image = countdown_window->images[countdown_window->current_image];

  const GSize icon_size = kino_reel_get_size(image);

  GPoint offset;
  offset.x = (layer->bounds.size.w / 2) - (icon_size.w / 2);
  offset.y = (layer->bounds.size.h / 2) - (icon_size.h / 2);
  kino_reel_draw(image, ctx, offset);
}

static void prv_timer_callback(void *data) {
  WorkoutCountdownWindow *countdown_window = data;

  if (countdown_window->current_image <= 0) {
    countdown_window->start_workout_cb(countdown_window->activity_type);
    window_stack_remove(&countdown_window->window, false);
    vibes_long_pulse();
    return;
  }

  countdown_window->current_image--;

  layer_mark_dirty(&countdown_window->base_layer);

  app_timer_register(TIMER_DURATION, prv_timer_callback, countdown_window);
}

static void prv_window_unload_handler(Window *window) {
  WorkoutCountdownWindow *countdown_window = window_get_user_data(window);
  if (countdown_window) {
    for (int i = 0; i < NUM_IMAGES; i++) {
      kino_reel_destroy(countdown_window->images[i]);
    }
    app_timer_cancel(countdown_window->timer);
    layer_deinit(&countdown_window->base_layer);
    window_deinit(&countdown_window->window);
    app_free(countdown_window);
  }
}

void workout_countdown_start(ActivitySessionType type, StartWorkoutCallback start_workout_cb) {
  WorkoutCountdownWindow *countdown_window = app_zalloc_check(sizeof(WorkoutCountdownWindow));

  countdown_window->start_workout_cb = start_workout_cb;
  countdown_window->activity_type = type;

  Window *window = &countdown_window->window;
  window_init(window, WINDOW_NAME("Workout Countdown"));
  window_set_user_data(window, countdown_window);
  window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorYellow, GColorDarkGray));
  window_set_window_handlers(window, &(WindowHandlers){
    .unload = prv_window_unload_handler,
  });

  layer_init(&countdown_window->base_layer, &window->layer.bounds);
  layer_set_update_proc(&countdown_window->base_layer, prv_base_layer_update_proc);
  layer_add_child(&window->layer, &countdown_window->base_layer);

  countdown_window->images[0] = kino_reel_create_with_resource(RESOURCE_ID_WORKOUT_APP_ONE);
  countdown_window->images[1] = kino_reel_create_with_resource(RESOURCE_ID_WORKOUT_APP_TWO);
  countdown_window->images[2] = kino_reel_create_with_resource(RESOURCE_ID_WORKOUT_APP_THREE);

  countdown_window->current_image = NUM_IMAGES - 1;

  app_timer_register(TIMER_DURATION, prv_timer_callback, countdown_window);

  app_window_stack_push(window, true);
}
