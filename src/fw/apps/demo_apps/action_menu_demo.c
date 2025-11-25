/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "action_menu_demo.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/action_menu_hierarchy.h"
#include "applib/ui/action_menu_window.h"
#include "applib/ui/action_menu_window_private.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "system/passert.h"
#include "util/size.h"

#include <string.h>

static struct {
  // main window
  Window *main_window;
  TextLayer *info_layer;
  // action menu
  ActionMenu *action_menu;
  // result_window
  Window *result_window;
  TextLayer *result_layer;
} *s_app_data;

///////////////////////
// Result Window

static void prv_result_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);

  s_app_data->result_layer = text_layer_create(GRect(0, 60, root_layer->bounds.size.w, 50));
  text_layer_set_text_alignment(s_app_data->result_layer, GTextAlignmentCenter);
  text_layer_set_text(s_app_data->result_layer, "Result!");
  layer_add_child(root_layer, (Layer *)s_app_data->result_layer);
}

static void prv_result_window_unload(Window *window) {
  window_destroy(window);
}

///////////////////////
// Action Menu Window

static void prv_action_menu_did_close_cb(ActionMenu *action_menu,
                                         const ActionMenuItem *item,
                                         void *context) {
  ActionMenuLevel *root_level = action_menu_get_root_level(action_menu);
  action_menu_hierarchy_destroy(root_level, NULL, NULL);
}

static void prv_action_callback(ActionMenu *action_menu,
                                const ActionMenuItem *action,
                                void *context) {
  s_app_data->result_window = window_create();
  window_set_window_handlers(s_app_data->result_window, &(WindowHandlers) {
    .load = prv_result_window_load,
    .unload = prv_result_window_unload,
  });
  action_menu_set_result_window(action_menu, s_app_data->result_window);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // First Level
  ActionMenuLevel *first_level = action_menu_level_create(10);
  action_menu_level_add_action(first_level,
                               "First!",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_action(first_level,
                               "Second!",
                               prv_action_callback,
                               NULL);

  // More Levels
  ActionMenuLevel *more_level = action_menu_level_create(1);
  action_menu_level_add_action(more_level,
                               "That's it, folks!",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_child(first_level,
                              more_level,
                              "More...");

  // Levels with multiple lines of text
  ActionMenuLevel *multiline_level = action_menu_level_create(5);
  action_menu_level_add_action(multiline_level,
                               "Sorry, I can't talk right now.",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_action(multiline_level,
                               "I can't talk just now, please text me if this is an emergency.",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_action(multiline_level,
                               "In a meeting, I will call you back when the meeting is over.",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_action(multiline_level,
                               "On my way, I will text you when I'm nearby.",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_action(multiline_level,
                               "I am busy.",
                               prv_action_callback,
                               NULL);
  action_menu_level_add_child(first_level,
                              multiline_level,
                              "Canned Responses");

  // Level with multi-column values of various row lengths
  ActionMenuLevel *multicolumn_select = action_menu_level_create(3);

  static const char* thin_values[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
                                       "M", "🍺" };
  ActionMenuLevel *multicolumn_one = action_menu_level_create(2);
  action_menu_level_set_display_mode(multicolumn_one, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < 2; i++) {
    action_menu_level_add_action(multicolumn_one, thin_values[i], prv_action_callback, NULL);
  }
  // ah ah ah
  ActionMenuLevel *multicolumn_two = action_menu_level_create(5);
  action_menu_level_set_display_mode(multicolumn_two, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < 5; i++) {
    action_menu_level_add_action(multicolumn_two, thin_values[i], prv_action_callback, NULL);
  }
  // ah ah ah
  ActionMenuLevel *multicolumn_many = action_menu_level_create(ARRAY_LENGTH(thin_values));
  action_menu_level_set_display_mode(multicolumn_many, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < ARRAY_LENGTH(thin_values); i++) {
    action_menu_level_add_action(multicolumn_many, thin_values[i], prv_action_callback, NULL);
  }
  // ah ah ah

  action_menu_level_add_child(multicolumn_select, multicolumn_one, "One row");
  action_menu_level_add_child(multicolumn_select, multicolumn_two, "Two rows");
  action_menu_level_add_child(multicolumn_select, multicolumn_many, "Many rows");
  action_menu_level_add_child(first_level, multicolumn_select, "Columns");

  first_level->separator_index = first_level->num_items - 1;

  ActionMenuConfig config = {
    .root_level = first_level,
    .context = NULL,
    .colors.background = GColorOxfordBlue,
    .colors.foreground = GColorOrange,
    .did_close = prv_action_menu_did_close_cb,
  };

  s_app_data->action_menu = app_action_menu_open(&config);
}

///////////////////////
// Main Window

static void prv_main_window_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

static void prv_main_window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);

  s_app_data->info_layer = text_layer_create(GRect(0, 60, root_layer->bounds.size.w, 50));
  text_layer_set_text_alignment(s_app_data->info_layer, GTextAlignmentCenter);
  text_layer_set_text(s_app_data->info_layer, "Press the select button");
  layer_add_child(root_layer, (Layer *)s_app_data->info_layer);
}

////////////////////
// App boilerplate

static void prv_init(void) {
  s_app_data = app_zalloc_check(sizeof(*s_app_data));

  s_app_data->main_window = window_create();
  window_set_window_handlers(s_app_data->main_window, &(WindowHandlers) {
    .load = prv_main_window_load,
  });
  window_set_click_config_provider(s_app_data->main_window, prv_main_window_click_config_provider);

  app_window_stack_push(s_app_data->main_window, true /* animated */);
}

static void s_main(void) {
  prv_init();

  app_event_loop();
}

const PebbleProcessMd* action_menu_demo_get_app_info() {
  static const PebbleProcessMdSystem s_app_data = {
    .common = {
      .main_func = s_main,
      // UUID: 101a32d95-ef69-46d4-a0b9-854cc62f97f9
      .uuid = {0x99, 0xa3, 0x2d, 0x95, 0xef, 0x69, 0x46, 0xd4,
               0xa0, 0xb9, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xf9},
    },
    .name = "Action Menu Demo",
  };
  return (const PebbleProcessMd*) &s_app_data;
}
