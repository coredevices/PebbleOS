/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_als_app.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "drivers/ambient_light.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "mfg/results_ui.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"

#include <stdint.h>
#include <stdio.h>

#define AMBIENT_READING_STR_LEN 32
typedef struct {
  Window *window;
  TextLayer *title_text_layer;
  TextLayer *reading_text_layer;
  char ambient_reading[AMBIENT_READING_STR_LEN];
  uint32_t latest_als_value;
#if MFG_INFO_RECORDS_TEST_RESULTS
  MfgResultsUI results_ui;
#endif
} AmbientLightAppData;

static void prv_update_reading(AmbientLightAppData *data) {
  uint32_t level = ambient_light_get_light_level();
  snprintf(data->ambient_reading, AMBIENT_READING_STR_LEN, "%"PRIu32, level);
  data->latest_als_value = level;
}

static void prv_timer_callback(void *cb_data) {
  AmbientLightAppData *data = app_state_get_user_data();

  prv_update_reading(data);
  layer_mark_dirty(window_get_root_layer(data->window));

  app_timer_register(500, prv_timer_callback, NULL);
}

#if MFG_INFO_RECORDS_TEST_RESULTS
static void prv_record_als_reading(void) {
  AmbientLightAppData *data = app_state_get_user_data();
  mfg_info_write_als_result(data->latest_als_value);
}
#endif

static void prv_handle_init(void) {
  AmbientLightAppData *data = task_zalloc_check(sizeof(AmbientLightAppData));

  data->window = window_create();

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;
  bounds.origin.y += 40;

  data->title_text_layer = text_layer_create(bounds);
  text_layer_set_font(data->title_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(data->title_text_layer, "ALS");
  text_layer_set_text_alignment(data->title_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(data->title_text_layer));

  bounds.origin.y += 30;
  data->reading_text_layer = text_layer_create(bounds);

  prv_update_reading(data);

  text_layer_set_font(data->reading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(data->reading_text_layer, data->ambient_reading);
  text_layer_set_text_alignment(data->reading_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(data->reading_text_layer));

#if MFG_INFO_RECORDS_TEST_RESULTS
  mfg_results_ui_init(&data->results_ui, MfgTest_ALS, data->window);
  mfg_results_ui_set_callback(&data->results_ui, prv_record_als_reading);
#endif

  app_state_set_user_data(data);
  app_window_stack_push(data->window, true);

  app_timer_register(10, prv_timer_callback, NULL);
}

static void prv_handle_deinit(void) {
  AmbientLightAppData *data = app_state_get_user_data();
  text_layer_destroy(data->title_text_layer);
  text_layer_destroy(data->reading_text_layer);
  window_destroy(data->window);
  task_free(data);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd* mfg_als_app_get_info(void) {
  static const PebbleProcessMdSystem s_ambient_light_info = {
    .common.main_func = prv_main,
    .name = "MfgALS"
  };
  return (const PebbleProcessMd*) &s_ambient_light_info;
}
