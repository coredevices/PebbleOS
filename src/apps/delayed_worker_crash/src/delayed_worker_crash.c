/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

typedef struct {
  Window *window;
  TextLayer *text_layer;
} DelayedWorkerCrashData;

static const char *WORKER_ALREADY_RUNNING = "Worker already running, crashing soon!";
static const char *WORKER_LAUNCHED = "Worker launched, will crash in 5 seconds!";
static const char *WORKER_LAUNCH_ERROR = "Error launching worker!";

static DelayedWorkerCrashData s_data;

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  char *message = NULL;

  // Check to see if the worker is currently active
  const bool running = app_worker_is_running();

  // Toggle running state
  if (running) {
    message = WORKER_ALREADY_RUNNING;
  } else {
    AppWorkerResult result = app_worker_launch();

    if (result == APP_WORKER_RESULT_SUCCESS) {
      message = WORKER_LAUNCHED;
    } else {
      message = WORKER_LAUNCH_ERROR;
    }
  }

  text_layer_set_text(s_data.text_layer, message);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_data.text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 500));
  text_layer_set_text(s_data.text_layer, "Click select to launch worker");
  text_layer_set_text_alignment(s_data.text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_data.text_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_data.text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_data.text_layer);
}

static void init(void) {
  s_data.window = window_create();
  window_set_click_config_provider(s_data.window, click_config_provider);
  window_set_window_handlers(s_data.window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(s_data.window, animated);
}

static void deinit(void) {
  window_destroy(s_data.window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
