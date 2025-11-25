/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/click.h"
#include "applib/ui/click_internal.h"
#include "applib/ui/window.h"

#include "util/attributes.h"

WEAK ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer) {
  return 0;
}

WEAK void click_recognizer_handle_button_up(ClickRecognizer *recognizer) {
  return;
}

WEAK void click_recognizer_handle_button_down(ClickRecognizer *recognizer) {
  return;
}

WEAK uint8_t click_number_of_clicks_counted(ClickRecognizerRef recognizer) {
  return 0;
}

WEAK bool click_recognizer_is_held_down(ClickRecognizerRef recognizer) {
  return false;
}

WEAK bool click_recognizer_is_repeating(ClickRecognizerRef recognizer) {
  return false;
}

WEAK void app_click_config_setup_with_window(ClickManager *click_manager, Window *window) {}
