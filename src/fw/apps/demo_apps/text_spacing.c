/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_spacing.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_management/sdk_shims.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

#include "applib/legacy2/ui/text_layer_legacy2.h"

typedef struct AppState {
  Window window;
  TextLayer text_layer;
  int16_t line_spacing_delta;
  GTextOverflowMode overflow_mode;
  GSize text_layer_size;
  GFont gothic_14_bold;
} AppState;

static const char* TEXT_DELTA_BUF =
  "!\"#$%&'()*+,-./ 0123456789:;<=>?@ ABCDEFGHIJKLMNOP QRSTUVWXYZ [\\]^_` "
  "abcdefghijklmnop qrstuvwxyz";

static void click_handler(ClickRecognizerRef recognizer, Window *window) {
  AppState *data = window_get_user_data(window);
  ButtonId button = click_recognizer_get_button_id(recognizer);
  if (button == BUTTON_ID_UP) {
    data->line_spacing_delta += 5;
    text_layer_set_line_spacing_delta(&data->text_layer, data->line_spacing_delta);
  } else if (button == BUTTON_ID_SELECT) {
    data->overflow_mode = (data->overflow_mode + 1) % 3;
    text_layer_set_overflow_mode(&data->text_layer, data->overflow_mode);
  } else if (button == BUTTON_ID_DOWN) {
    if (data->line_spacing_delta < 5) {
      data->line_spacing_delta -= 1;
    } else {
      data->line_spacing_delta -= 5;
    }
    text_layer_set_line_spacing_delta(&data->text_layer, data->line_spacing_delta);
  }

  GSize size_used = text_layer_get_content_size(app_get_current_graphics_context(),
                                                &data->text_layer);
  PBL_LOG(LOG_LEVEL_DEBUG, "Line Delta: %d, Size %d x %d, Overflow: %d", data->line_spacing_delta,
          size_used.w, size_used.h, data->overflow_mode);
}

static void config_provider(Window *window) {
  window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler)click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler)click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler)click_handler);
  (void)window;
}

static void prv_window_load(Window *window) {
  AppState *data = window_get_user_data(window);
  data->text_layer_size = GSize(144, 168);
  text_layer_init(&data->text_layer,
                  &GRect(0, 0, data->text_layer_size.w, data->text_layer_size.h));

  text_layer_set_background_color(&data->text_layer, GColorWhite);
  text_layer_set_text_color(&data->text_layer, GColorBlack);

  text_layer_set_text(&data->text_layer, TEXT_DELTA_BUF);
  data->gothic_14_bold = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  text_layer_set_font(&data->text_layer, data->gothic_14_bold);
  text_layer_set_text_alignment(&data->text_layer, GTextAlignmentCenter);
  data->line_spacing_delta = 0;
  text_layer_set_line_spacing_delta(&data->text_layer, data->line_spacing_delta);
  data->overflow_mode = GTextOverflowModeWordWrap;
  text_layer_set_overflow_mode(&data->text_layer, data->overflow_mode);

  layer_add_child(&window->layer, &data->text_layer.layer);

  GSize size_used = text_layer_get_content_size(app_get_current_graphics_context(),
                                                &data->text_layer);
  PBL_LOG(LOG_LEVEL_DEBUG, "Max size used %d %d", size_used.w, size_used.h);
}

static void push_window(struct AppState *data) {
  Window* window = &data->window;
  window_init(window, WINDOW_NAME("Text Spacing"));
  window_set_user_data(window, data);
  window_set_click_config_provider(window, (ClickConfigProvider) config_provider);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}


////////////////////
// App boilerplate

static void handle_init(void) {
  struct AppState* data = app_malloc_check(sizeof(struct AppState));

  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  struct AppState* data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* text_spacing_app_get_info() {
  static const PebbleProcessMdSystem text_delta_info = {
    .common.main_func = &s_main,
    .name = "Text Spacing" // The first 4 bytes is a UTF-8 codepoint for the hamster emoji.
  };
  return (const PebbleProcessMd*) &text_delta_info;
}
