/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

#define ALLOC_SIZE  2048

static Window *window;
static TextLayer *text_heap_info;

static unsigned s_alloc_total = 0;
static char s_text_buf[80];

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  char *buf = malloc(ALLOC_SIZE);
  if (buf == NULL) {
    snprintf(s_text_buf, 80, "Heap full at %dB", s_alloc_total);
    text_layer_set_text(text_heap_info, s_text_buf);
    return;
  }
  s_alloc_total += ALLOC_SIZE;
  snprintf(s_text_buf, 80, "%dB allocated", s_alloc_total);
  text_layer_set_text(text_heap_info, s_text_buf);
}

static void config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void init() {
  window = window_create();
  window_set_click_config_provider(window, config_provider);
  window_stack_push(window, true /* Animated */);
  Layer *window_layer = window_get_root_layer(window);

  text_heap_info = text_layer_create(layer_get_frame(window_layer));
  text_layer_set_text_color(text_heap_info, GColorWhite);
  text_layer_set_background_color(text_heap_info, GColorBlack);
  text_layer_set_font(text_heap_info, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  snprintf(s_text_buf, 80, "Press [SELECT] to allocate %dB", ALLOC_SIZE);
  text_layer_set_text(text_heap_info, s_text_buf);
  layer_add_child(window_layer, text_layer_get_layer(text_heap_info));
}

static void deinit(void) {
  // Don't free anything
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
