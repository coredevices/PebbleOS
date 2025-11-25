/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "mfg_func_test_buttons.h"
#include "applib/ui/ui.h"

static void black_window_button_up(ClickRecognizerRef recognizer, Window *window) {
  MfgFuncTestData *app_data = (MfgFuncTestData*)window_get_user_data(window);

  mfg_func_test_append_bits(MfgFuncTestBitBlackTestPassed);
  app_data->black_test_done = true;
  vibes_short_pulse();

  const bool animated = false;
  window_stack_pop(animated);
}

static void black_window_click_config_provider(void *context) {
  for (ButtonId button_id = BUTTON_ID_BACK; button_id < NUM_BUTTONS; ++button_id) {
    window_raw_click_subscribe(button_id, NULL, (ClickHandler) black_window_button_up, context);
  }
}

static void push_black_test_window(MfgFuncTestData *app_data) {
  Window *black_window = &app_data->black_window;
  window_init(black_window, WINDOW_NAME("Mfg Func Test Black"));
  window_set_background_color(black_window, GColorBlack);
  window_set_click_config_provider_with_context(black_window,
      (ClickConfigProvider) black_window_click_config_provider, black_window);
  window_set_user_data(black_window, app_data);
  window_set_fullscreen(black_window, true);

  const bool animated = false;
  window_stack_push(black_window, animated);
}
