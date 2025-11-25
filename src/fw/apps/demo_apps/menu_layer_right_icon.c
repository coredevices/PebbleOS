/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "menu_layer_right_icon.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "system/logging.h"
#include "system/passert.h"

#include <stdio.h>


#define NUM_MENU_SECTIONS 1
#define NUM_MENU_ITEMS 4

typedef struct {
  Window window;
  MenuLayer menu_layer;
  GBitmap checked_icon;
} AppData;

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index,
                                           void *data) {
  return NUM_MENU_ITEMS;
}

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index,
                                               void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index,
                                   void *data) {
  AppData *app_data = (AppData *) data;
  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw_icon_right(ctx, cell_layer, "First Item", NULL,
                                      &app_data->checked_icon);
      break;
    case 1:
      menu_cell_basic_draw_icon_right(ctx, cell_layer, "Second Item", NULL,
                                      &app_data->checked_icon);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Third Item", NULL,
                                      &app_data->checked_icon);
      break;
    case 3:
      menu_cell_basic_draw_icon_right(ctx, cell_layer, "Fourth Item", "with a subtitle",
                                      &app_data->checked_icon);
      break;
  }
}

static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  return;
}

static int16_t menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                      void *callback_context) {
  switch(cell_index->row) {
    case 0: return menu_cell_small_cell_height();
    case 1: return menu_cell_basic_cell_height();
    case 2: return menu_cell_small_cell_height();
    case 3: return menu_cell_basic_cell_height();
    default:
      return menu_cell_basic_cell_height();
  }
}

static void prv_window_load(Window *window) {
  PBL_LOG(LOG_LEVEL_INFO, "WINDOW LOADING");
  AppData *data = window_get_user_data(window);
  gbitmap_init_with_resource(&data->checked_icon, RESOURCE_ID_CHECKBOX_ICON_CHECKED);

  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &window->layer.bounds);
  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_sections = (MenuLayerGetNumberOfSectionsCallback) menu_get_num_sections_callback,
    .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) menu_get_num_rows_callback,
    .get_cell_height = (MenuLayerGetCellHeightCallback) menu_get_cell_height_callback,
    .draw_row = (MenuLayerDrawRowCallback) menu_draw_row_callback,
    .select_click = menu_select_callback,
  });
  menu_layer_set_highlight_colors(&data->menu_layer, GColorJaegerGreen, GColorWhite);
  menu_layer_set_click_config_onto_window(menu_layer, window);
  layer_add_child(&window->layer, menu_layer_get_layer(menu_layer));
}

static void push_window(AppData *data) {
  PBL_LOG(LOG_LEVEL_INFO, "PUSHING WINDOW");
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Demo Menu"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}

////////////////////
// App boilerplate

static void handle_init() {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit() {
  AppData *data = app_state_get_user_data();
  menu_layer_deinit(&data->menu_layer);
  app_free(data);
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd* menu_layer_right_icon_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "MenuLayer Right Icon Demo"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
