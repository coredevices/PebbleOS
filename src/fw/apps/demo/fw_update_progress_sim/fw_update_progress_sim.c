/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fw_update_progress_sim.h"

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/progress_layer.h"
#include "applib/ui/text_layer.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"

#include <stdio.h>
#include <stdint.h>

#define UPDATE_FREQ_MS 75
#define COMPLETE_PAUSE_MS 1000
#define PROG_LAYER_START_VAL 6
// Used to force the progress bar to start at PROG_LAYER_START_VAL and scale
// the rest of the progress between that value and MAX_PROGRESS_PERCENT
#define PROG_LAYER_TRANSFORM(real_prog) \
    (PROG_LAYER_START_VAL + (real_prog * \
     (MAX_PROGRESS_PERCENT - PROG_LAYER_START_VAL) / MAX_PROGRESS_PERCENT))

typedef struct {
  Window window;
  TextLayer percent_done_text_layer;
  char percent_done_text_buffer[5];
  ProgressLayer progress_layer;
  AppTimer *timer;
  unsigned int percent_complete;
} FwUpdateProgressSimData;

static void prv_update_progress_text(FwUpdateProgressSimData *data) {
  sniprintf(data->percent_done_text_buffer,
            sizeof(data->percent_done_text_buffer), "%u%%", data->percent_complete);
  layer_mark_dirty(&data->percent_done_text_layer.layer);
}

static void prv_refresh_progress(void *data_in) {
  FwUpdateProgressSimData *data = data_in;
  if (!data) {
    return;
  }

  uint32_t delay = UPDATE_FREQ_MS;
  if (data->percent_complete >= MAX_PROGRESS_PERCENT) {
    data->percent_complete = 0;
    delay = COMPLETE_PAUSE_MS;
  } else {
    data->percent_complete++;
  }

  prv_update_progress_text(data);
  progress_layer_set_progress(&data->progress_layer,
                              PROG_LAYER_TRANSFORM(data->percent_complete));

  data->timer = app_timer_register(delay, prv_refresh_progress, data);
}

static void prv_window_load_handler(Window *window) {
  FwUpdateProgressSimData *data = window_get_user_data(window);

  const int16_t load_bar_length = 108;
  const int16_t load_bar_height = 8;
  const int16_t x_offset = (window->layer.bounds.size.w - load_bar_length) / 2;
  const int16_t y_offset_progress = (window->layer.bounds.size.h - load_bar_height) / 2;
  const int16_t y_offset_text = y_offset_progress - 38;
  const GRect progress_bounds = GRect(x_offset, y_offset_progress, load_bar_length, load_bar_height);
  ProgressLayer *progress_layer = &data->progress_layer;
  progress_layer_init(progress_layer, &progress_bounds);
  progress_layer_set_corner_radius(progress_layer, 3);
  layer_add_child(&window->layer, &progress_layer->layer);

  TextLayer *percent_done_text_layer = &data->percent_done_text_layer;
  text_layer_init_with_parameters(percent_done_text_layer,
                                  &GRect(0, y_offset_text, window->layer.bounds.size.w, 30),
                                  data->percent_done_text_buffer,
                                  fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                                  GColorBlack, GColorClear, GTextAlignmentCenter,
                                  GTextOverflowModeTrailingEllipsis);
  layer_add_child(&window->layer, &percent_done_text_layer->layer);

  data->percent_complete = 0;
  prv_update_progress_text(data);
  progress_layer_set_progress(progress_layer, PROG_LAYER_TRANSFORM(data->percent_complete));
  data->timer = app_timer_register(UPDATE_FREQ_MS, prv_refresh_progress, data);
}

static void prv_window_unload_handler(Window *window) {
  FwUpdateProgressSimData *data = window_get_user_data(window);
  if (data && data->timer) {
    app_timer_cancel(data->timer);
    data->timer = NULL;
  }
}

static void prv_handle_init(void) {
  FwUpdateProgressSimData *data = app_zalloc_check(sizeof(FwUpdateProgressSimData));
  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("FW Update Progress Sim"));
  window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load_handler,
    .unload = prv_window_unload_handler,
  });
  app_window_stack_push(window, true);
}

static void prv_handle_deinit(void) {
  FwUpdateProgressSimData *data = app_state_get_user_data();
  app_free(data);
}

static void prv_main(void) {
  prv_handle_init();

  app_event_loop();

  prv_handle_deinit();
}

const PebbleProcessMd *fw_update_progress_sim_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &prv_main,
    .name = "FW Update Progress Sim",
  };
  return (const PebbleProcessMd *) &s_app_info;
}
