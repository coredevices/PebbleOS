/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "scroll_app.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

typedef struct {
  Window window;
  ScrollLayer scroll_layer;
  TextLayer text;
  InverterLayer inverter;
} ScrollAppData;

static void select_click_handler(ClickRecognizerRef recognizer, ScrollAppData *data) {
  PBL_LOG(LOG_LEVEL_DEBUG, "SELECT clicked!");
  (void)data;
  (void)recognizer;
}

#if 0
static void select_long_click_handler(ClickRecognizerRef recognizer, ScrollAppData *data) {
  PBL_LOG(LOG_LEVEL_DEBUG, "SELECT long clicked!");
  (void)data;
  (void)recognizer;
}
#endif

static void click_config_provider(ScrollAppData *data) {
  // The config that gets passed in, has already the UP and DOWN buttons configured
  // to scroll up and down. It's possible to override that here, if needed.

  // Configure how the SELECT button should behave:
  window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, (ClickHandler) select_click_handler, NULL);
}

static void prv_window_load(Window *window) {
  ScrollAppData *data = window_get_user_data(window);

  ScrollLayer *scroll_layer = &data->scroll_layer;
  scroll_layer_init(scroll_layer, &window->layer.bounds);
  scroll_layer_set_click_config_onto_window(scroll_layer, window);
  scroll_layer_set_callbacks(scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = (ClickConfigProvider) click_config_provider,
  });
  scroll_layer_set_context(scroll_layer, data);
  scroll_layer_set_content_size(scroll_layer, GSize(window->layer.bounds.size.w, 500));

  const GRect max_text_bounds = GRect(0, 0, window->layer.bounds.size.w, 500);
  TextLayer *text = &data->text;
  text_layer_init(text, &max_text_bounds);
  text_layer_set_text(text, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam quam tellus, fermentum quis vulputate quis, vestibulum interdum sapien. Vestibulum lobortis pellentesque pretium. Quisque ultricies purus eu orci convallis lacinia. Cras a urna mi. Donec convallis ante id dui dapibus nec ullamcorper erat egestas. Aenean a mauris a sapien commodo lacinia. Sed posuere mi vel risus congue ornare. Curabitur leo nisi, euismod ut pellentesque sed, suscipit sit amet lorem. Aliquam eget sem vitae sem aliquam ornare. In sem sapien, imperdiet eget pharetra a, lacinia ac justo. Suspendisse at ante nec felis facilisis eleifend.");

  // Trim text layer and scroll content to fit text box
  GSize max_size = text_layer_get_content_size(app_state_get_graphics_context(), text);
  text_layer_set_size(text, max_size);
  static const int vert_scroll_padding = 4;
  scroll_layer_set_content_size(scroll_layer, GSize(window->layer.bounds.size.w,
                                                    max_size.h + vert_scroll_padding));

  scroll_layer_add_child(scroll_layer, &text->layer);

  InverterLayer *inverter = &data->inverter;
  inverter_layer_init(inverter, &GRect(15, 15, 30, 30));
  scroll_layer_add_child(scroll_layer, &inverter->layer);

  layer_add_child(&window->layer, &scroll_layer->layer);
}

static void push_window(ScrollAppData *data) {
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Scroll Demo"));
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
  ScrollAppData *data = app_malloc_check(sizeof(ScrollAppData));

  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  ScrollAppData *data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* scroll_app_get_info() {
  static const PebbleProcessMdSystem s_scroll_app_info = {
    .common.main_func = &s_main,
    .name = "Scroller"
  };
  return (const PebbleProcessMd*) &s_scroll_app_info;
}

