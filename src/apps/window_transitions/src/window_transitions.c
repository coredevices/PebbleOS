/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

static bool s_next_window_fullscreen;

static void unload_handler(Window *window) {
  window_destroy(window);
}

static void push_window(void);

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  push_window();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void push_window(void) {
  Window *window = window_create();
  window_set_fullscreen(window, s_next_window_fullscreen);
  window_set_window_handlers(window, (WindowHandlers) {
    .unload = unload_handler,
  });
  window_set_click_config_provider(window, click_config_provider);

  s_next_window_fullscreen = !s_next_window_fullscreen;

  window_stack_push(window, true);
}

int main(void) {
  push_window();

  app_event_loop();
}
