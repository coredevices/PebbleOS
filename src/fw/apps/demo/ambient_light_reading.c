/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "drivers/ambient_light.h"
#include "kernel/pbl_malloc.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"

#include <stdint.h>
#include <stdio.h>

#define AMBIENT_READING_STR_LEN 32
typedef struct {
  Window *window;
  TextLayer *text_layer;
  char ambient_reading[AMBIENT_READING_STR_LEN];
} AmbientLightAppData;

static void prv_populate_amb_read_str(char *str) {
  int level = ambient_light_get_light_level();
  snprintf(str, AMBIENT_READING_STR_LEN, "Amb Level:\n %d", level);
}

static void timer_callback(void *cb_data) {
  AmbientLightAppData *data = app_state_get_user_data();

  prv_populate_amb_read_str(&data->ambient_reading[0]);
  layer_mark_dirty(window_get_root_layer(data->window));

  app_timer_register(500, timer_callback, NULL);
}

static void handle_init(void) {
  AmbientLightAppData *data = task_malloc_check(sizeof(AmbientLightAppData));

  data->window = window_create();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  data->text_layer = text_layer_create((GRect)
      { .origin = { 0, 40 }, .size = { bounds.size.w, 100 } });

  prv_populate_amb_read_str(&data->ambient_reading[0]);

  text_layer_set_font(data->text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(data->text_layer, data->ambient_reading);
  text_layer_set_text_alignment(data->text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(data->text_layer));

  app_state_set_user_data(data);
  app_window_stack_push(data->window, true);

  app_timer_register(10, timer_callback, NULL);
}

static void handle_deinit(void) {
  AmbientLightAppData *data = app_state_get_user_data();
  text_layer_destroy(data->text_layer);
  window_destroy(data->window);
  task_free(data);
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd* ambient_light_reading_get_info() {
  static const PebbleProcessMdSystem s_ambient_light_info = {
    .common.main_func = s_main,
    .name = "Amb Reading"
  };
  return (const PebbleProcessMd*) &s_ambient_light_info;
}
