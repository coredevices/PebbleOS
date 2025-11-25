/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/button.h"
#include "drivers/backlight.h"

#include "util/trig.h"

#include "applib/ui/ui.h"
#include "applib/ui/window_private.h"

#include "applib/fonts/fonts.h"

typedef struct {
  MfgFuncTestData *app_data;
  ButtonId button_id;
  TextLayer label;
  PathLayer arrow;
} ButtonTestData;

static const GPathInfo ARROW_PATH_INFO = {
  .num_points = 7,
  .points = (GPoint []) {{0, 14}, {29, 14}, {29, 0}, {54, 25}, {29, 50}, {29, 36}, {0, 36}}
};

static const GPathInfo BOLT_PATH_INFO = {
  .num_points = 6,
  .points = (GPoint []) {{21, 0}, {14, 26}, {28, 26}, {7, 60}, {14, 34}, {0, 34}}
};

static void move_arrow_to_button(ButtonTestData *data, ButtonId id) {
#define ARROW_SIZE {54, 50}
  static const GRect ARROW_RECTS[] = {
    {{2, 30}, ARROW_SIZE},   // BACK
    {{88, 2}, ARROW_SIZE},   // UP
    {{88, 59}, ARROW_SIZE},  // SELECT
    {{88, 116}, ARROW_SIZE}, // DOWN
  };
  layer_set_frame(&data->arrow.layer, &ARROW_RECTS[id]);
  gpath_rotate_to(&data->arrow.path, id == BUTTON_ID_BACK ? (TRIG_MAX_ANGLE / 2) : 0);
  gpath_move_to(&data->arrow.path, id == BUTTON_ID_BACK ? GPoint(54, 50) : GPoint(0, 0));
}

static void button_window_button_up(ClickRecognizerRef recognizer, Window *window) {
  ButtonTestData *data = window_get_user_data(window);
  ButtonId button_id = click_recognizer_get_button_id(recognizer);

  if (data->button_id == button_id) {
    ++(data->button_id);
    if (data->button_id > BUTTON_ID_DOWN) {
      mfg_func_test_append_bits(MfgFuncTestBitButtonTestPassed);
      data->app_data->button_test_done = true;
      data->button_id = BUTTON_ID_BACK;
      const bool animated = false;
      window_stack_pop(animated);
    } else {
      move_arrow_to_button(data, data->button_id);
    }
    backlight_set_brightness(0);
  }
}

static void button_window_button_down(ClickRecognizerRef recognizer, Window *window) {
  ButtonTestData *data = window_get_user_data(window);
  if (data->button_id == click_recognizer_get_button_id(recognizer)) {
    backlight_set_brightness(0xffff);
  }
}

static void button_window_click_config_provider(void *context) {
  for (ButtonId button_id = BUTTON_ID_BACK; button_id < NUM_BUTTONS; ++button_id) {
    window_raw_click_subscribe(button_id,
        (ClickHandler) button_window_button_down, (ClickHandler) button_window_button_up, context);
  }
}

static void button_window_load(Window *window) {
  ButtonTestData *data = window_get_user_data(window);
  Layer *root = &window->layer;

  TextLayer *label = &data->label;
  text_layer_init(label, GRect(0, 0, 144, 40));
  text_layer_set_background_color(label, GColorClear);
  text_layer_set_text_color(label, GColorBlack);
  text_layer_set_text(label, "Press Button");
  text_layer_set_font(label, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(root, &label->layer);

  PathLayer *arrow = &data->arrow;
  path_layer_init(arrow, &ARROW_PATH_INFO);
  path_layer_set_fill_color(arrow, GColorBlack);
  path_layer_set_stroke_color(arrow, GColorClear);
  layer_add_child(root, &arrow->layer);

  move_arrow_to_button(data, data->button_id);
}

static void push_button_test_window(MfgFuncTestData *app_data) {
  static ButtonTestData s_button_test_data = {
    .button_id = BUTTON_ID_BACK,
  };

  s_button_test_data.app_data = app_data;

  Window *button_window = &app_data->button_window;
  window_init(button_window, WINDOW_NAME("Mfg Func Test Buttons"));
  window_set_overrides_back_button(button_window, true);
  window_set_click_config_provider_with_context(button_window,
      (ClickConfigProvider) button_window_click_config_provider, button_window);
  window_set_window_handlers(button_window, &(WindowHandlers) {
      .load = button_window_load
  });
  window_set_user_data(button_window, &s_button_test_data);
  window_set_fullscreen(button_window, true);
  const bool animated = false;
  window_stack_push(button_window, animated);
}
