/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "services/common/light.h"
#include "drivers/led_controller.h"

#define BACKLIGHT_COLOR_WHITE       0xD0D0D0
#define BACKLIGHT_COLOR_RED         0xFF0000
#define BACKLIGHT_COLOR_GREEN       0x00FF00
#define BACKLIGHT_COLOR_BLUE        0x0000FF
#define BACKLIGHT_COLOR_BLACK       0x000000

typedef enum {
  TestPattern_White,
  TestPattern_Red,
  TestPattern_Green,
  TestPattern_Blue,
  TestPattern_Black,
  NumTestPatterns
} TestPattern;

typedef struct {
  Window window;
  TestPattern test_pattern;
} AppData;


static void prv_update_proc(struct Layer *layer, GContext* ctx) {
  AppData *app_data = app_state_get_user_data();
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &layer->bounds);

  PBL_LOG(LOG_LEVEL_INFO, "backlight id:%d", app_data->test_pattern);
  switch (app_data->test_pattern) {
  case TestPattern_White:
    led_controller_rgb_set_color(BACKLIGHT_COLOR_WHITE);
    break;
  case TestPattern_Red:
    led_controller_rgb_set_color(BACKLIGHT_COLOR_RED);
    break;
  case TestPattern_Green:
    led_controller_rgb_set_color(BACKLIGHT_COLOR_GREEN);
    break;
  case TestPattern_Blue:
    led_controller_rgb_set_color(BACKLIGHT_COLOR_BLUE);
    break;
  case TestPattern_Black:
    led_controller_rgb_set_color(BACKLIGHT_COLOR_BLACK);
    break;
  default:
    break;
  }
}

static void prv_button_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  app_data->test_pattern = (app_data->test_pattern + 1) % NumTestPatterns;

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_change_pattern(void *data) {
  AppData *app_data = app_state_get_user_data();

  app_data->test_pattern = (TestPattern) data;

  layer_mark_dirty(&app_data->window.layer);
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_button_click_handler);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .test_pattern = (TestPattern) app_manager_get_task_context()->args
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  Layer *layer = window_get_root_layer(window);
  layer_set_update_proc(layer, prv_update_proc);

  app_window_stack_push(window, true /* Animated */);
}

static void s_main(void) {
  light_enable(true);

  prv_handle_init();

  app_event_loop();

  led_controller_rgb_set_color(0xd0d0d0);
  light_enable(false);
}

const PebbleProcessMd* mfg_backlight_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 2d825a20-2fa3-4b26-be6f-ab41d1280c73
    .common.uuid = { 0x2d, 0x82, 0x5a, 0x20, 0x2f, 0xa3, 0x4b, 0x26,
                     0xbe, 0x6f, 0xab, 0x41, 0xd1, 0x28, 0x0c, 0x73 },
    .name = "MfgBacklight",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

