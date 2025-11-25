/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/app_logging.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/simple_menu_layer.h"
#include "applib/ui/ui.h"
#include "drivers/temperature.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "util/math.h"
#include "util/size.h"

#include "temperature_demo.h"

#include <stdio.h>

#define USE_FAKE_DATA 0

#define CUR_TEMP_HEIGHT 35
#define CUR_TEMP_TOP 1
#define CUR_TEMP_RECT GRect(0, CUR_TEMP_TOP, DISP_COLS, CUR_TEMP_HEIGHT)

#define TEMP_RANGE_HEIGHT 20
#define TEMP_RANGE_TOP CUR_TEMP_HEIGHT
#define TEMP_RANGE_RECT GRect(0, TEMP_RANGE_TOP, DISP_COLS, TEMP_RANGE_HEIGHT)

#define PLOT_TOP 60
#define PLOT_BOTTOM DISP_ROWS
#define PLOT_HEIGHT (PLOT_BOTTOM - PLOT_TOP)
#define PLOT_WIDTH DISP_COLS
#define PLOT_RECT GRect(0, PLOT_TOP, PLOT_WIDTH, PLOT_HEIGHT)

#define READ_HISTORY_ENTRIES (4 * MINUTES_PER_HOUR)
static int32_t s_temp_readings[READ_HISTORY_ENTRIES];

// Demo temperature
typedef struct {
  Window *window;
  TextLayer *cur_temp_layer;
  TextLayer *temp_range_layer;
  char cur_temp_text[32];
  char temp_range_text[128];
  int min_temp;
  int max_temp;
} TemperatureDemoAppData;

static TemperatureDemoAppData *s_data;


// -------------------------------------------------------------------------------
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

// -------------------------------------------------------------------------------
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
}

// -------------------------------------------------------------------------------
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
}


// -------------------------------------------------------------------------------
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

// -------------------------------------------------------------------------------
static void cur_temp_update_text(TemperatureDemoAppData *data) {
  // Show current temp
  int cur_temp = temperature_read();
  snprintf(data->cur_temp_text, sizeof(data->cur_temp_text), "%d", cur_temp);
  text_layer_set_text(data->cur_temp_layer, data->cur_temp_text);

  snprintf(data->temp_range_text, sizeof(data->temp_range_text),
           "%d - %d", data->min_temp, data->max_temp);
  text_layer_set_text(data->temp_range_layer, data->temp_range_text);

  layer_mark_dirty(window_get_root_layer(data->window));
}

// -------------------------------------------------------------------------------
static void handle_second_tick(struct tm* tick_time, TimeUnits units_changed) {
  int32_t reading = temperature_read();
  memcpy(s_temp_readings, s_temp_readings + 1, (READ_HISTORY_ENTRIES - 1) * sizeof(int32_t));
  s_temp_readings[READ_HISTORY_ENTRIES - 1] = reading;
  cur_temp_update_text(s_data);
}

// -------------------------------------------------------------------------------
static void layer_update_proc(Layer *layer, GContext* ctx) {
  TemperatureDemoAppData *data = s_data;
  const GRect *bounds = &layer->bounds;

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds);

  // Plot temperature readings
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);

#if USE_FAKE_DATA
  // TEST
  for (int i = 0; i < (int)READ_HISTORY_ENTRIES; i++) {
    int delta = (i % 50);
    if (delta > 25) {
      delta = 50 - delta;
    }
    s_temp_readings[i] = 1600 + delta;
  }
#endif

  // Draw the last bunch that fit in the window
  int first_idx = (int)READ_HISTORY_ENTRIES - PLOT_WIDTH;
  first_idx = MAX(first_idx, 0);

  // Get the min and max
  bool first_valid = true;
  int first_non_zero_idx = -1;
  for (int i = first_idx; i < (int)READ_HISTORY_ENTRIES; i++) {
    if (s_temp_readings[i] == 0) {
      continue;
    }
    if (first_non_zero_idx < 0) {
      first_non_zero_idx = i;
    }
    if (first_valid) {
      data->min_temp = data->max_temp = s_temp_readings[i];
      first_valid = false;
    } else {
      data->min_temp = MIN(data->min_temp, s_temp_readings[i]);
      data->max_temp = MAX(data->max_temp, s_temp_readings[i]);
    }
  }
  if (first_non_zero_idx < 0) {
    return;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "min temp: %d, max temp: %d", data->min_temp, data->max_temp);
  int temp_range = data->max_temp - data->min_temp;
  temp_range = MAX(10, temp_range);

  for (int i = first_non_zero_idx, x_pos = 0; i < (int)READ_HISTORY_ENTRIES; i++, x_pos++) {
    int reading = s_temp_readings[i];
    int line_height = (reading - data->min_temp) * PLOT_HEIGHT / temp_range;
    int y_pos = PLOT_BOTTOM - line_height;

    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_draw_line(ctx, GPoint(x_pos, PLOT_BOTTOM), GPoint(x_pos, y_pos));

    GRect dot = GRect(x_pos, y_pos, 4, 4);
    graphics_fill_rect(ctx, &dot);
    graphics_draw_pixel(ctx, GPoint(x_pos, PLOT_BOTTOM - line_height));
  }
}

// -------------------------------------------------------------------------------
static void prv_window_load(Window *window) {
  TemperatureDemoAppData *data = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);
  // GRect bounds = window_layer->bounds;

  layer_set_update_proc(window_layer, layer_update_proc);

  // Current temp
  data->cur_temp_layer = text_layer_create(CUR_TEMP_RECT);
  text_layer_set_text_alignment(data->cur_temp_layer, GTextAlignmentCenter);
  text_layer_set_font(data->cur_temp_layer,
                      fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS));
  text_layer_set_background_color(data->cur_temp_layer, GColorClear);
  text_layer_set_text_color(data->cur_temp_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(data->cur_temp_layer));

  // Current temp range
  data->temp_range_layer = text_layer_create(TEMP_RANGE_RECT);
  text_layer_set_text_alignment(data->temp_range_layer, GTextAlignmentCenter);
  text_layer_set_font(data->temp_range_layer,
                      fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS));
  text_layer_set_background_color(data->temp_range_layer, GColorClear);
  text_layer_set_text_color(data->temp_range_layer, GColorBlack);
  layer_add_child(window_layer, text_layer_get_layer(data->temp_range_layer));

  // Update UI
  cur_temp_update_text(data);

  // Subscribe to second tick for updates
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}


// -------------------------------------------------------------------------------
static void prv_window_unload(Window *window) {
  TemperatureDemoAppData *data = window_get_user_data(window);
  text_layer_destroy(data->cur_temp_layer);
}


// -------------------------------------------------------------------------------
static void deinit(void) {
  TemperatureDemoAppData *data = app_state_get_user_data();
  window_destroy(data->window);
  app_free(data);
}


// -------------------------------------------------------------------------------
static void init(void) {
  TemperatureDemoAppData *data = app_zalloc_check(sizeof(TemperatureDemoAppData));
  s_data = data;
  app_state_set_user_data(data);

  // Init window
  data->window = window_create();
  window_set_background_color(data->window, GColorWhite);
  window_set_user_data(data->window, data);
  window_set_click_config_provider_with_context(data->window, click_config_provider, data);
  window_set_window_handlers(data->window, &(WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  app_window_stack_push(data->window, true /* Animated */);
}


// -------------------------------------------------------------------------------
static void s_main(void) {
  init();
  app_event_loop();
  deinit();
}


// -------------------------------------------------------------------------------
const PebbleProcessMd* temperature_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_temperature_demo_app_info = {
    .common.main_func = &s_main,
    .name = "Temperature"
  };
  return (const PebbleProcessMd*) &s_temperature_demo_app_info;
}
