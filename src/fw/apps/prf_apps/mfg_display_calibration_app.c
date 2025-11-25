/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/window.h"
#include "applib/ui/app_window_stack.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_serials.h"
#include "mfg/mfg_info.h"
#include "services/common/light.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "resource/system_resource.h"

#include <stdio.h>

#define TICK_LENGTH 20
#define LINE_HEIGHT 20

typedef enum {
  X_AdjustState,
  Y_AdjustState,

  NumAdjustStates
} AppState;

typedef struct {
  Window window;

  AppTimer *exit_timer;
  AppState app_state;
  int8_t axis_offsets[NumAdjustStates];

  char text_buffer[16];
  char device_serial[MFG_SERIAL_NUMBER_SIZE + 1];
  bool is_saving;
} AppData;

static void prv_draw_solid(Layer *layer, GContext *ctx, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, &layer->bounds);
}

static void prv_display_offsets(Layer *layer, GContext *ctx) {
  AppData *data = app_state_get_user_data();
  graphics_context_set_text_color(ctx, GColorWhite);

  const GRect *bounds = &data->window.layer.bounds;
  int pixel_max = bounds->origin.x + bounds->size.w - 1;

  GFont font;
  for (int i = 0; i < NumAdjustStates; i++) {
    font = fonts_get_system_font((i == data->app_state) ? FONT_KEY_GOTHIC_24_BOLD
                                                        : FONT_KEY_GOTHIC_24);

    snprintf(data->text_buffer, sizeof(data->text_buffer), "%c: %"PRIi8, 'X' + i,
             data->axis_offsets[X_AdjustState + i]);
    graphics_draw_text(ctx, data->text_buffer, font,
                       GRect(bounds->origin.x, bounds->origin.y + 35 + LINE_HEIGHT * i,
                             pixel_max, pixel_max),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void prv_display_serial_number(Layer *layer, GContext *ctx) {
  AppData *data = app_state_get_user_data();

  const GRect *bounds = &data->window.layer.bounds;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, data->device_serial, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds->origin.x, (bounds->origin.x + bounds->size.w) / 2,
                           bounds->size.w, bounds->size.h),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_display_saving_message(Layer *layer, GContext *ctx) {
  AppData *data = app_state_get_user_data();

  const GRect *bounds = &data->window.layer.bounds;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "Saving...", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(bounds->origin.x, (bounds->origin.y + bounds->size.h) / 2 + LINE_HEIGHT,
                           bounds->size.w, bounds->size.h),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_draw_crosshair(Layer *layer, GContext *ctx) {
  AppData *data = app_state_get_user_data();

  const GRect *bounds = &data->window.layer.bounds;
  const int mid_pixel_minus_one = (bounds->origin.x + bounds->size.w - 1) / 2;
  const int pixel_min = bounds->origin.x;
  const int pixel_max = bounds->origin.x + bounds->size.w - 1;

  for (int i  = 0; i < 2; i++) {
    graphics_context_set_stroke_color(ctx, i ? GColorWhite : GColorWhite);

    graphics_draw_line(ctx, (GPoint) {.x = mid_pixel_minus_one + i, .y = pixel_min},
                            (GPoint) {.x = mid_pixel_minus_one + i, .y = pixel_min + TICK_LENGTH});
    graphics_draw_line(ctx, (GPoint) {.x = mid_pixel_minus_one + i, .y = pixel_max - TICK_LENGTH},
                            (GPoint) {.x = mid_pixel_minus_one + i, .y = pixel_max});
    graphics_draw_line(ctx, (GPoint) {.x = pixel_min, .y = mid_pixel_minus_one + i},
                            (GPoint) {.x = pixel_min + TICK_LENGTH, .y = mid_pixel_minus_one + i});
    graphics_draw_line(ctx, (GPoint) {.x = pixel_max - TICK_LENGTH, .y = mid_pixel_minus_one + i},
                            (GPoint) {.x = pixel_max, .y = mid_pixel_minus_one + i});
  }
}

static void prv_draw_border_stripes(Layer *layer, GContext *ctx) {
  AppData *data = app_state_get_user_data();

  const GRect *bounds = &data->window.layer.bounds;
  const int pixel_min = bounds->origin.x;
  const int pixel_max = bounds->origin.x + bounds->size.w - 1;

  for (int j = 0; j < 2; j++) {
    graphics_context_set_stroke_color(ctx, j ? GColorGreen : GColorRed);
    for (int i = 0 + j; i < 5; i += 2) {
      graphics_draw_line(ctx, (GPoint) {.x = i, .y = pixel_min},
                              (GPoint) {.x = i, .y = pixel_max});
      graphics_draw_line(ctx, (GPoint) {.x = pixel_max - i, .y = pixel_min},
                              (GPoint) {.x = pixel_max - i, .y = pixel_max});
      graphics_draw_line(ctx, (GPoint) {.x = pixel_min, .y = i},
                              (GPoint) {.x = pixel_max, .y = i});
      graphics_draw_line(ctx, (GPoint) {.x = pixel_min, .y = pixel_max - i},
                              (GPoint) {.x = pixel_max, .y = pixel_max - i});
    }
  }
}

static void prv_layer_update_proc(Layer *layer, GContext *ctx) {
  AppData *data = app_state_get_user_data();

  prv_draw_solid(layer, ctx, GColorBlack);

  ctx->draw_state.drawing_box.origin.x += data->axis_offsets[X_AdjustState];
  ctx->draw_state.drawing_box.origin.y += data->axis_offsets[Y_AdjustState];

  prv_draw_border_stripes(layer, ctx);
  prv_draw_crosshair(layer, ctx);

  prv_display_offsets(layer, ctx);
  prv_display_serial_number(layer, ctx);
  if (data->is_saving) {
    prv_display_saving_message(layer, ctx);
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  app_data->app_state = (app_data->app_state + 1) % (NumAdjustStates);

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  --(app_data->axis_offsets[app_data->app_state]);

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  ++(app_data->axis_offsets[app_data->app_state]);

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_save_offsets_callback(void *data) {
  AppData *app_data = app_state_get_user_data();

  mfg_info_set_disp_offsets((GPoint) {
    .x = app_data->axis_offsets[X_AdjustState],
    .y = app_data->axis_offsets[Y_AdjustState]
  });

  app_window_stack_pop_all(false);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  if (app_data->axis_offsets[X_AdjustState] == mfg_info_get_disp_offsets().x &&
      app_data->axis_offsets[Y_AdjustState] == mfg_info_get_disp_offsets().y) {
    app_window_stack_pop_all(true);
    return;
  }

  app_data->is_saving = true;
  layer_mark_dirty(&app_data->window.layer);

  app_data->exit_timer = app_timer_register(200, prv_save_offsets_callback, NULL);
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .app_state = (AppState) app_manager_get_task_context()->args,
    .axis_offsets[X_AdjustState] = mfg_info_get_disp_offsets().x,
    .axis_offsets[Y_AdjustState] = mfg_info_get_disp_offsets().y
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "MfgDisplayCalibration");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  Layer *layer = window_get_root_layer(window);
  layer_set_update_proc(layer, prv_layer_update_proc);

  mfg_info_get_serialnumber(data->device_serial, sizeof(data->device_serial));

  light_enable(true);

  app_window_stack_push(window, true /* Animated */);
}

static void prv_handle_deinit(void) {
  light_enable(false);

  AppData *data = app_state_get_user_data();
  app_timer_cancel(data->exit_timer);
  app_free(data);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  prv_handle_deinit();
}

const PebbleProcessMd* mfg_display_calibration_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: d0582042-5beb-410f-9fed-76eccd31821e
    .common.uuid = { 0xd0, 0x58, 0x20, 0x42, 0x5b, 0xeb, 0x41, 0x0f,
                     0x9f, 0xed, 0x76, 0xec, 0xcd, 0x31, 0x82, 0x1e },
    .name = "MfgDisplayCalibration",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
