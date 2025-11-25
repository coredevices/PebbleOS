/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "menu_app.h"

#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "system/passert.h"

#include <stdio.h>

#define BUFFER_SIZE 25

static const uint8_t s_music_launcher_icon_pixels[] = {
  0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x01, 0x00, 0xff, 0x3f, 0x00, 0x00, 0xff, 0x03, 0x00, 0x00, /* bytes 0 - 16 */
  0x7f, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x18, 0x00, 0x7f, 0x00, 0x1f, 0x00, /* bytes 16 - 32 */
  0x7f, 0xf0, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, /* bytes 32 - 48 */
  0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, /* bytes 48 - 64 */
  0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x1f, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x7f, 0x7c, 0x00, 0x00, /* bytes 64 - 80 */
  0x03, 0x3c, 0x00, 0x00, 0x01, 0x3c, 0x00, 0x00, 0x00, 0x3c, 0x80, 0x00, 0x00, 0x3c, 0xc0, 0x00, /* bytes 80 - 96 */
  0x00, 0x7e, 0xe0, 0x00, 0x00, 0xff, 0xff, 0x00, 0x81, 0xff, 0xff, 0x00,
};

static const GBitmap s_music_launcher_icon_bitmap = {
  .addr = (void*) &s_music_launcher_icon_pixels,
  .row_size_bytes = 4,
  .info_flags = 0x1000,
  .bounds = {
    .origin = { .x = 0, .y = 0 },
    .size = { .w = 24, .h = 27 },
  },
};

typedef struct {
  Window window;
  MenuLayer menu_layer;
  GBitmap icon;

  Window detail_window;
  TextLayer detail_text;
  char detail_text_buffer[50];
} AppData;

static uint16_t get_num_sections_callback(struct MenuLayer *menu_layer, AppData *data) {
  (void)data;
  (void)menu_layer;
  return 4;
}

static uint16_t get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, AppData *data) {
  (void)data;
  (void)menu_layer;
  switch (section_index) {
    default:
    case 0: return 2;
    case 1: return 3;
    case 2: return 4;
    case 3: return 5;
  }
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, AppData *data) {
  (void)data;
  (void)menu_layer;
  (void)cell_index;

  // Variable row heights demo:
  switch (cell_index->row % 3) {
    default:
    case 0:
      return 44;

    case 1:
      return 64;

    case 2:
      return 84;
  }
}

static int16_t get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, AppData *data) {
  (void)data;
  (void)menu_layer;
  (void)section_index;
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, AppData *data) {
  char title[BUFFER_SIZE];
  char subtitle[BUFFER_SIZE];

  switch (cell_index->row % 2) {
    case 0:
      sniprintf(title, BUFFER_SIZE, "Title %i/%i ", cell_index->section, cell_index->row);
      sniprintf(subtitle, BUFFER_SIZE, "Subtitle %i/%i", cell_index->section, cell_index->row);
      menu_cell_basic_draw(ctx, cell_layer, title, subtitle, (GBitmap*)&s_music_launcher_icon_bitmap);
      break;

    default:
    case 1:
      sniprintf(title, BUFFER_SIZE, "Only Title %i/%i", cell_index->section, cell_index->row);
      menu_cell_title_draw(ctx, cell_layer, title);
      break;
  }
  (void)data;
}

static void draw_header_callback(GContext* ctx, Layer *cell_layer, uint16_t section_index, AppData *data) {
  char title[BUFFER_SIZE];
  sniprintf(title, BUFFER_SIZE, "Section Header (%i)", section_index);
  menu_cell_basic_header_draw(ctx, cell_layer, title);
  (void)data;
}

static void detail_window_load(Window *window) {
  AppData *data = window_get_user_data(window);
  TextLayer *text_layer = &data->detail_text;
  text_layer_init(text_layer, &window->layer.bounds);
  text_layer_set_text(text_layer, data->detail_text_buffer);
  layer_add_child(&window->layer, &text_layer->layer);
}

static void push_detail_window(AppData *data, MenuIndex *index, bool is_long_click) {
  sniprintf(data->detail_text_buffer, 50, "SELECTION:\n\nSection %i, Row %i\nLong click: %c", index->section, index->row, is_long_click ? 'Y' : 'N');

  Window *detail_window = &data->detail_window;
  window_init(detail_window, WINDOW_NAME("Demo Menu Detail"));
  window_set_user_data(detail_window, data);
  window_set_window_handlers(detail_window, &(WindowHandlers) {
    .load = detail_window_load,
  });
  const bool animated = true;
  app_window_stack_push(detail_window, animated);
}

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, AppData *data) {
  push_detail_window(data, cell_index, false);
  (void)menu_layer;
}

static void select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, AppData *data) {
  push_detail_window(data, cell_index, true);
  (void)menu_layer;
}

static void prv_window_load(Window *window) {
  AppData *data = window_get_user_data(window);

  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &window->layer.bounds);
  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_sections = (MenuLayerGetNumberOfSectionsCallback) get_num_sections_callback,
    .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
    .get_cell_height = (MenuLayerGetCellHeightCallback) get_cell_height_callback,
    .get_header_height = (MenuLayerGetHeaderHeightCallback) get_header_height_callback,
    .draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
    .draw_header = (MenuLayerDrawHeaderCallback) draw_header_callback,
    .select_click = (MenuLayerSelectCallback) select_callback,
    .select_long_click = (MenuLayerSelectCallback) select_long_callback,
  });
  menu_layer_set_click_config_onto_window(menu_layer, window);
  layer_add_child(&window->layer, menu_layer_get_layer(menu_layer));
}

static void push_window(AppData *data) {
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

static void handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  menu_layer_deinit(&data->menu_layer);
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* menu_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "MenuLayer Demo"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

#undef BUFFER_SIZE
