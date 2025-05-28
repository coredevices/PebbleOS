/*
 * Copyright 2025 Core Devices LLC
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
#include "util/trig.h"
#include "applib/app_watch_info.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/text_layer.h"
#include "kernel/pbl_malloc.h"
#include "mfg/mfg_info.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "util/bitset.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#if PLATFORM_ASTERIX
const char *const s_model = "C2D";
#else
const char *const s_model = "Unknown";
#endif

typedef struct {
  WatchInfoColor color;
  const char *name;
  const char *short_name;
} ColorTable;

const ColorTable s_color_table[] = {
#if PLATFORM_ASTERIX
  {
    .color = WATCH_INFO_COLOR_COREDEVICES_C2D_BLACK,
    .name = "BLACK",
    .short_name = "BK",
  },
  {
    .color = WATCH_INFO_COLOR_COREDEVICES_C2D_WHITE,
    .name = "WHITE",
    .short_name = "WH",
  }
#endif
};

typedef struct {
  Window window;

  TextLayer title;
  TextLayer color;
  TextLayer status;

  int selected_color_index;
} AppData;

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  if (app_data->selected_color_index == 0) {
    return;
  }

  app_data->selected_color_index--;
  text_layer_set_text(&app_data->color,
                      s_color_table[app_data->selected_color_index].name);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();

  if (app_data->selected_color_index == ARRAY_LENGTH(s_color_table) - 1) {
    return;
  }

  app_data->selected_color_index++;
  text_layer_set_text(&app_data->color,
                      s_color_table[app_data->selected_color_index].name);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *data) {
  AppData *app_data = app_state_get_user_data();
  char model[MFG_INFO_MODEL_STRING_LENGTH];

  if (app_data->selected_color_index == -1) {
    return;
  }
  
  snprintf(model, sizeof(model), "%s-%s",
           s_model, s_color_table[app_data->selected_color_index].short_name);

  mfg_info_set_model(model);
  mfg_info_set_watch_color(s_color_table[app_data->selected_color_index].color);

  text_layer_set_text(&app_data->status, "PROGRAMMED!");
}

static void prv_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .selected_color_index = -1,
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "PROGRAM COLOR");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *color = &data->color;
  text_layer_init(color,
                  &GRect(5, 70,
                         window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 70));
  text_layer_set_font(color, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(color, GTextAlignmentCenter);
  if (ARRAY_LENGTH(s_color_table) > 0) {
    data->selected_color_index = 0;
    text_layer_set_text(color, s_color_table[0].name);
  } else {
    text_layer_set_text(color, "NO COLORS AVAILABLE");
  }
  layer_add_child(&window->layer, &color->layer);

  TextLayer *status = &data->status;
  text_layer_init(status,
                  &GRect(5, 110,
                         window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 110));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  layer_add_child(&window->layer, &status->layer);

  app_window_stack_push(window, true /* Animated */);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_program_color_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: d5f0a47d-e570-499d-bcaa-fc6d56230038
    .common.uuid = { 0xd5, 0xf0, 0xa4, 0x7d, 0xe5, 0x70, 0x49, 0x9d,
                     0xbc, 0xaa, 0xfc, 0x6d, 0x56, 0x23, 0x00, 0x38 },
    .name = "MfgProgramColor",
  };
  return (const PebbleProcessMd*) &s_app_info;
}

