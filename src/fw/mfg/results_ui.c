/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "results_ui.h"

#include "applib/ui/app_window_stack.h"

static void prv_record_and_exit(MfgResultsUI *results_ui, bool result) {
  mfg_info_write_test_result(results_ui->test, result);

  if (results_ui->results_cb) {
    results_ui->results_cb();
  }

  app_window_stack_pop(true);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *data) {
  prv_record_and_exit(data, true);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *data) {
  prv_record_and_exit(data, false);
}

static void prv_click_config_provider(void *data) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

void mfg_results_ui_init(MfgResultsUI *results_ui, MfgTest test, Window *window) {
  GRect bounds = window->layer.bounds;
  bounds.size.w -= 5;
  bounds.size.h = 40;
  bounds.origin.y += 5;

  TextLayer *pass = &results_ui->pass_text_layer;
  text_layer_init(pass, &bounds);
  text_layer_set_font(pass, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(pass, GTextAlignmentRight);
  text_layer_set_text(pass, "Pass");
  layer_add_child(&window->layer, &pass->layer);

  bounds.origin.y = 120;
  TextLayer *fail = &results_ui->fail_text_layer;
  text_layer_init(fail, &bounds);
  text_layer_set_font(fail, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(fail, GTextAlignmentRight);
  text_layer_set_text(fail, "Fail");
  layer_add_child(&window->layer, &fail->layer);

  results_ui->test = test;

  window_set_click_config_provider_with_context(window, prv_click_config_provider, results_ui);
}

void mfg_results_ui_set_callback(MfgResultsUI *ui, MfgResultsCallback cb) {
  ui->results_cb = cb;
}
