/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "progress_app.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "util/math.h"

#define PROGRESS_STEP 2

typedef struct {
  Window window;
  ProgressLayer progress_layer;
  int progress;
} ProgressAppData;

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Reset
  ProgressAppData *data = context;
  data->progress = MIN_PROGRESS_PERCENT;
  progress_layer_set_progress(&data->progress_layer, data->progress);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Increment
  ProgressAppData *data = context;
  data->progress = MIN(MAX_PROGRESS_PERCENT, data->progress + PROGRESS_STEP);
  progress_layer_set_progress(&data->progress_layer, data->progress);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Decrement
  ProgressAppData *data = context;
  data->progress = MAX(MIN_PROGRESS_PERCENT, data->progress - PROGRESS_STEP);
  progress_layer_set_progress(&data->progress_layer, data->progress);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  ProgressAppData *data = window_get_user_data(window);
  data->progress = (MIN_PROGRESS_PERCENT + MAX_PROGRESS_PERCENT) / 2;

  ProgressLayer* progress_layer = &data->progress_layer;

  const GRect *frame = &window_get_root_layer(window)->frame;

  static const uint32_t MARGIN = 20;
  static const uint32_t HEIGHT = PBL_IF_COLOR_ELSE(6, 7);
  const GRect progress_bounds = GRect(MARGIN, (frame->size.h - HEIGHT) / 2,
                                      frame->size.w - (2 * MARGIN), HEIGHT);
  progress_layer_init(progress_layer, &progress_bounds);
  progress_layer_set_progress(progress_layer, data->progress);
  layer_add_child(&window->layer, &progress_layer->layer);

  progress_layer_set_corner_radius(progress_layer, PBL_IF_COLOR_ELSE(2, 3));
}

static void handle_init(void) {
  ProgressAppData *data = app_zalloc_check(sizeof(ProgressAppData));
  app_state_set_user_data(data);

  Window* window = &data->window;
  window_init(window, WINDOW_NAME("Progress Demo"));
  window_set_user_data(window, data);
  window_set_click_config_provider_with_context(window, prv_click_config_provider, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}

static void handle_deinit(void) {
  struct AppState* data = app_state_get_user_data();
  tick_timer_service_unsubscribe();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* progress_app_get_info() {
  static const PebbleProcessMdSystem progress_app_info = {
    .common.main_func = &s_main,
    .name = "Progress Bar Test"
  };
  return (const PebbleProcessMd*) &progress_app_info;
}

