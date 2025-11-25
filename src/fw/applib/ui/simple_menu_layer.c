/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "simple_menu_layer.h"

#include "applib/legacy2/ui/menu_layer_legacy2.h"
#include "applib/applib_malloc.auto.h"
#include "process_management/process_manager.h"

static int16_t get_header_height(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
  (void)menu_layer;
  if (((SimpleMenuLayer*)callback_context)->sections[section_index].title) {
    return MENU_CELL_BASIC_HEADER_HEIGHT;
  } else {
    return 0;
  }
}

static uint16_t get_num_sections(MenuLayer *menu_layer, void *callback_context) {
  (void)menu_layer;
  return ((SimpleMenuLayer*)callback_context)->num_sections;
}

static uint16_t get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
  (void)menu_layer;
  return ((SimpleMenuLayer*)callback_context)->sections[section_index].num_items;
}

static void draw_row(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
  SimpleMenuLayer *simple_menu = (SimpleMenuLayer*)callback_context;

  const SimpleMenuItem *item = &simple_menu->sections[cell_index->section].items[cell_index->row];

  menu_cell_basic_draw(ctx, cell_layer, item->title, item->subtitle, item->icon);
}

static void draw_header(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context) {
  const char *title = ((SimpleMenuLayer*)callback_context)->sections[section_index].title;
  if (title == NULL) {
    return;
  }
  menu_cell_basic_header_draw(ctx, cell_layer, title);
}

static void select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  (void)menu_layer;
  SimpleMenuLayer *simple_menu = (SimpleMenuLayer*)callback_context;

  SimpleMenuLayerSelectCallback cb = simple_menu->sections[cell_index->section].items[cell_index->row].callback;
  if (cb != NULL) {
    cb(cell_index->row, simple_menu->callback_context);
  }
}


void simple_menu_layer_init(SimpleMenuLayer *simple_menu, const GRect *frame, Window *window,
                            const SimpleMenuSection *sections, int num_sections,
                            void *callback_context) {
  if (process_manager_compiled_with_legacy2_sdk()) {
    menu_layer_legacy2_init(&simple_menu->menu, frame);
  } else {
    menu_layer_init(&simple_menu->menu, frame);
  }

  simple_menu->sections = sections;
  simple_menu->num_sections = num_sections;
  simple_menu->callback_context = callback_context;

  // use this SimpleMenuLayer as the callback context
  menu_layer_set_callbacks(&simple_menu->menu, simple_menu, &(MenuLayerCallbacks) {
    .get_num_sections = get_num_sections,
    .get_header_height = get_header_height,
    .get_num_rows = get_num_rows,
    .draw_row = draw_row,
    .select_click = select_click,
    .draw_header = draw_header,
  });

  menu_layer_set_click_config_onto_window(&simple_menu->menu, window);
}

SimpleMenuLayer* simple_menu_layer_create(GRect frame, Window *window,
                                          const SimpleMenuSection *sections, int32_t num_sections,
                                          void *callback_context) {
  SimpleMenuLayer *layer = applib_type_malloc(SimpleMenuLayer);
  if (layer) {
    simple_menu_layer_init(layer, &frame, window, sections, num_sections, callback_context);
  }
  return layer;
}

void simple_menu_layer_deinit(SimpleMenuLayer *menu_layer) {
  menu_layer_deinit(&menu_layer->menu);
}

void simple_menu_layer_destroy(SimpleMenuLayer* menu_layer) {
  if (menu_layer == NULL) {
    return;
  }
  simple_menu_layer_deinit(menu_layer);
  applib_free(menu_layer);
}

Layer* simple_menu_layer_get_layer(const SimpleMenuLayer *simple_menu) {
  return menu_layer_get_layer(&simple_menu->menu);
}

int simple_menu_layer_get_selected_index(const SimpleMenuLayer *simple_menu) {
  return menu_layer_get_selected_index(&simple_menu->menu).row;
}

void simple_menu_layer_set_selected_index(SimpleMenuLayer *simple_menu, int32_t index, bool animated) {
  MenuIndex menu_index = MenuIndex(simple_menu->menu.selection.index.section, index);
  menu_layer_set_selected_index(&simple_menu->menu, menu_index, MenuRowAlignCenter, animated);
}

MenuLayer *simple_menu_layer_get_menu_layer(SimpleMenuLayer *simple_menu) {
  return &simple_menu->menu;
}
