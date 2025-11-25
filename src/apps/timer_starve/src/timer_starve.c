/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

static const int FPS_NO_PROBLEM = 10;
static const int FPS_NO_RESPONSE = 20;

#define FPS 80

static Window *window;

static void timed_update(void *data) {
  layer_mark_dirty(window_get_root_layer(window));
  app_timer_register(1000 / FPS, timed_update, NULL);
}

static void init(void) {
  window = window_create();

  Layer *window_layer = window_get_root_layer(window);

  GRect window_bounds = layer_get_bounds(window_layer);

  TextLayer *text_layer = text_layer_create(window_bounds);
  text_layer_set_text(text_layer, "Unplug and plug in the charger. You will see that the system cannot keep up with it.");
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  text_layer = text_layer_create((GRect) {{ 0, window_bounds.size.h / 2 }, window_bounds.size} );
  static char buffer[80];
  snprintf(buffer, sizeof(buffer), "FPS: %u", FPS);
  text_layer_set_text(text_layer, buffer);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  window_stack_push(window, true);

  timed_update(NULL);
}

int main(void) {
  init();
  app_event_loop();
}

