/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "menu_overflow_app.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "applib/ui/menu_layer.h"


static Window *window;
static MenuLayer *menu_layer;
static char *section_names[] = { "Movies", "Books", "Video Games", "Television", "Alcohol" };
static char *row_names[5][2] = {
  { "Avengers", "Eden of the East" },
  { "A Song of Ice and Fire", "Lord of the Rings" },
  { "Team Fortress 2", "Super Meat Boy" },
  { "Sunny in Philadelphia", "Gotham" },
  { "Beer", "Vodka" }
};


////////////////////
// MenuLayer construction and callback

static uint16_t prv_menu_get_num_sections_callback(struct MenuLayer *menu_layer,
    void *callback_context) {
  return 5;
}

static uint16_t prv_menu_get_num_rows_callback(struct MenuLayer *menu_layer,
    uint16_t section_index, void *callback_context) {
  return 2;
}

static int16_t prv_menu_get_header_height_callback(struct MenuLayer *menu_layer,
    uint16_t section_index, void *callback_context) {
  return 15;
}

static int16_t prv_menu_get_cell_height_callback(struct MenuLayer *menu_layer,
    MenuIndex *cell_index, void *callback_context) {
  return 20;
}

static int16_t prv_menu_get_separator_height_callback(struct MenuLayer *menu_layer,
    MenuIndex *cell_index, void *callback_context) {
  return 10;
}

static void prv_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer,
    uint16_t section_index, void *callback_context) {
  menu_cell_basic_header_draw(ctx, cell_layer, section_names[section_index]);
}

static void prv_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer,
    MenuIndex *cell_index, void *callback_context) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, row_names[cell_index->section][cell_index->row],
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(4, 2, 136, 22),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

////////////////////
// App boilerplate

static void init(void) {
  window = window_create();

  menu_layer = menu_layer_create(window_get_root_layer(window)->bounds);
  menu_layer_set_callbacks(menu_layer, NULL, &(MenuLayerCallbacks) {
    .get_num_sections = prv_menu_get_num_sections_callback,
    .get_num_rows = prv_menu_get_num_rows_callback,
    .get_header_height = prv_menu_get_header_height_callback,
    .get_cell_height = prv_menu_get_cell_height_callback,
    .get_separator_height = prv_menu_get_separator_height_callback,
    .draw_header = prv_menu_draw_header_callback,
    .draw_row = prv_menu_draw_row_callback,
  });

  menu_layer_set_click_config_onto_window(menu_layer, window);
  layer_add_child(window_get_root_layer(window), menu_layer_get_layer(menu_layer));

  app_window_stack_push(window, true);
}

static void deinit(void) {
  menu_layer_destroy(menu_layer);
  window_destroy(window);
}

static void s_main(void) {
  init();

  app_event_loop();

  deinit();
}

const PebbleProcessMd* menu_overflow_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Menu Overflow"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
