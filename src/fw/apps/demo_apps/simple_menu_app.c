/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "simple_menu_app.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "services/common/poll_remote.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"

typedef struct {
  Window window;
  SimpleMenuLayer menu;
} AppData;

static void callback_a(int index, void *ctx) {
  (void)index;
  (void)ctx;
  PBL_LOG(LOG_LEVEL_DEBUG, "A called back");
}

static void other_callback(int index, void *ctx) {
  (void)ctx;
  PBL_LOG(LOG_LEVEL_DEBUG, "other callback: %d", index);
}

static void poll_callback(int index, void *ctx) {
  poll_remote_send_request(POLL_REMOTE_SERVICE_MAIL);
}

static const SimpleMenuItem s_menu_items[] = {
  { "Poll Mail", "", NULL, poll_callback },
  { "Title A", "Callback A", NULL, callback_a },
  { "Another Title", NULL, NULL, other_callback },
  { "Last Title", "Last subtitle", NULL, other_callback }
};

static const SimpleMenuSection s_menu_sections[] = {{ .title = NULL, .items = s_menu_items, .num_items = ARRAY_LENGTH(s_menu_items) }};

static void prv_window_load(Window *window) {
  AppData *data = window_get_user_data(window);

  SimpleMenuLayer *menu = &data->menu;
  simple_menu_layer_init(menu, &(GRect){{0, 0}, window->layer.frame.size}, window, s_menu_sections,
                         ARRAY_LENGTH(s_menu_sections), data);
  layer_add_child(&window->layer, simple_menu_layer_get_layer(menu));
}

static void handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Simple Menu Demo"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load
  });

  const bool animated = true;
  app_window_stack_push(window, animated);
}

static void handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  simple_menu_layer_deinit(&data->menu);
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* simple_menu_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "SimpleMenuLayer Demo"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

