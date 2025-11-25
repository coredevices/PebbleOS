/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_flow.h"

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_management/sdk_shims.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

typedef struct AppState {
  Window window;
  ScrollLayer scroll_layer;
  TextLayer text_layer_1;
  TextLayer text_layer_2;
  TextLayer box;
} AppState;

static const char* QUOTE_BUF_1 =
  "Space, the final frontier. These are the voyages of the starship Enterprise. "
  "Its 5-year mission: to explore strange new worlds, "
  "to seek out new life and new civilizations, to boldly go where no man has gone before. ";

static const char* QUOTE_BUF_2 =
  "Dib: You're just jealous...\n"
  "Zim: This has nothing to do with jelly!\n"
  "Zim: You dare agree with me? Prepare to meet your horrible doom!";

static void prv_window_load(Window *window) {
  graphics_text_perimeter_debugging_enable(true);

  AppState *data = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);

  int16_t y_offset = 48;
  int16_t page_height = 85;  // height of scroll layer, also used for paging height

  // Initialize the scroll layer
  const GRect scroll_bounds = GRect(0, y_offset, DISP_ROWS, page_height);
  scroll_layer_init(&data->scroll_layer, &scroll_bounds);

  // Use a text layer to show the Scroll Layer background area
  text_layer_init(&data->box, &scroll_bounds);
  text_layer_set_background_color(&data->box, GColorLightGray);
  layer_add_child(window_layer, text_layer_get_layer(&data->box));

  layer_add_child(window_layer, scroll_layer_get_layer(&data->scroll_layer));

  // This binds the scroll layer to the window so that up and down map to scrolling
  // You may use scroll_layer_set_callbacks to add or override interactivity
  scroll_layer_set_click_config_onto_window(&data->scroll_layer, window);

  // Initialize the first text layer
  text_layer_init(&data->text_layer_1, &GRect(0, 20, scroll_bounds.size.w, 2000));
  text_layer_set_text(&data->text_layer_1, QUOTE_BUF_1);
  text_layer_set_background_color(&data->text_layer_1, GColorYellow);
  text_layer_set_font(&data->text_layer_1, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(&data->text_layer_1, GTextAlignmentCenter);

  // Add first text layer to scroll layer (required to get correct content size based on location)
  const int inset = 8;
  const int padding = 4;
  scroll_layer_add_child(&data->scroll_layer, text_layer_get_layer(&data->text_layer_1));
  text_layer_enable_screen_text_flow_and_paging(&data->text_layer_1, inset);
  const GSize max_size_1 =
      text_layer_get_content_size(app_state_get_graphics_context(), &data->text_layer_1);
  text_layer_set_size(&data->text_layer_1, GSize(scroll_bounds.size.w, max_size_1.h + padding));

  // Initialize the second text layer
  text_layer_init(&data->text_layer_2, &GRect(0, 20 + max_size_1.h, scroll_bounds.size.w, 2000));
  text_layer_set_text(&data->text_layer_2, QUOTE_BUF_2);
  text_layer_set_background_color(&data->text_layer_2, GColorCyan);
  text_layer_set_font(&data->text_layer_2, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(&data->text_layer_2, GTextAlignmentCenter);

  // Add second text layer to scroll layer (required to get correct content size based on location)
  scroll_layer_add_child(&data->scroll_layer, text_layer_get_layer(&data->text_layer_2));
  text_layer_enable_screen_text_flow_and_paging(&data->text_layer_2, inset);
  const GSize max_size_2 =
      text_layer_get_content_size(app_state_get_graphics_context(), &data->text_layer_2);
  text_layer_set_size(&data->text_layer_1, GSize(scroll_bounds.size.w, max_size_2.h + padding));

  // Setup paging before getting content size
  scroll_layer_set_paging(&data->scroll_layer, true);
  // Trim text layer and scroll content to fit text box
  scroll_layer_set_content_size(
    &data->scroll_layer, GSize(scroll_bounds.size.w, max_size_1.h + max_size_2.h));
}

static void push_window(struct AppState *data) {
  Window* window = &data->window;
  window_init(window, WINDOW_NAME("Text Flow"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}


////////////////////
// App boilerplate

static void handle_init(void) {
  struct AppState* data = app_zalloc_check(sizeof(struct AppState));

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

const PebbleProcessMd* text_flow_app_get_info() {
  static const PebbleProcessMdSystem text_flow_info = {
    .common.main_func = &s_main,
    .name = "Text Flow"
  };
  return (const PebbleProcessMd*) &text_flow_info;
}
