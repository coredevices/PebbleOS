/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

static Window *window;
static TextLayer *time_layer;
static TextLayer *gmtime_layer;
static TextLayer *localtime_layer;


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  time_t the_time = time(NULL);

  // Time layer
  static char time_buf[32];
  snprintf(time_buf, 32, "time: %u", (unsigned) the_time);

  time_layer = text_layer_create(GRect(0, 0, 144, 168));
  text_layer_set_text(time_layer, time_buf);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(time_layer));

  // Time layer
  struct tm *gm_time = gmtime(&the_time);
  static char gmtime_buf[32];
  snprintf(gmtime_buf, 32, "gmtime: %d:%02d, is_dst: %d",
      gm_time->tm_hour, gm_time->tm_min, gm_time->tm_isdst);

  gmtime_layer = text_layer_create(GRect(0, 40, 144, 168));
  text_layer_set_text(gmtime_layer, gmtime_buf);
  text_layer_set_font(gmtime_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(gmtime_layer));

  char gmtime_strftime_buf[32];
  strftime(gmtime_strftime_buf, 32, "%z %Z", gm_time);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "gmtime: %s", gmtime_strftime_buf);

  // Time layer
  struct tm *lt_time = localtime(&the_time);
  static char localtime_buf[32];
  snprintf(localtime_buf, 32, "localtime: %d:%02d, is_dst: %d",
      lt_time->tm_hour, lt_time->tm_min, lt_time->tm_isdst);

  localtime_layer = text_layer_create(GRect(0, 96, 144, 168));
  text_layer_set_text(localtime_layer, localtime_buf);
  text_layer_set_font(localtime_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(localtime_layer));

  char localtime_strftime_buf[32];
  strftime(localtime_strftime_buf, 32, "%z %Z", lt_time);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "localtime: %s", localtime_strftime_buf);
}

static void window_unload(Window *window) {
  text_layer_destroy(time_layer);
  text_layer_destroy(gmtime_layer);
  text_layer_destroy(localtime_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
