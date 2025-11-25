/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "persist_app.h"

#include "applib/app.h"
#include "process_state/app_state/app_state.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"

#include "applib/persist.h"

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

static const uint32_t COUNT_PKEY = 1;

static uint16_t get_num_sections_callback(struct MenuLayer *menu_layer, AppData *data) {
  (void)data;
  (void)menu_layer;
  return 1;
}

static uint16_t get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, AppData *data) {
  (void)data;
  (void)menu_layer;
  switch (section_index) {
    default:
    case 0: return 3;
  }
}

static int16_t get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, AppData *data) {
  (void)data;
  (void)menu_layer;
  (void)section_index;
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, AppData *data) {
  (void)data;
  switch (cell_index->row) {
    case 0: {
      int num_beers = persist_read_int(COUNT_PKEY);
      char title[50];
      snprintf(title, sizeof(title), "%d Bottles", num_beers);
      menu_cell_basic_draw(ctx, cell_layer, title, "of beer on the wall", (GBitmap*)&s_music_launcher_icon_bitmap);
      break;
    }
    case 1:
      menu_cell_title_draw(ctx, cell_layer, "Order More");
      break;
    case 2:
      menu_cell_title_draw(ctx, cell_layer, "Drink!");
      break;
  }
}

static void draw_header_callback(GContext* ctx, Layer *cell_layer, uint16_t section_index, AppData *data) {
  (void)section_index;
  (void)data;
  menu_cell_basic_header_draw(ctx, cell_layer, "Beer Counter");
}

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, AppData *data) {
  (void)menu_layer;
  (void)data;
  switch (cell_index->row) {
    case 1: {
      int num_beers = persist_read_int(COUNT_PKEY);
      int status = persist_write_int(COUNT_PKEY, num_beers+1);
      PBL_LOG(LOG_LEVEL_DEBUG, "argh %d %d", num_beers, status);
      menu_layer_reload_data(menu_layer);
      break;
    }
    case 2: {
      int num_beers = persist_read_int(COUNT_PKEY);
      persist_write_int(COUNT_PKEY, num_beers-1);
      menu_layer_reload_data(menu_layer);
      break;
    }
  }
}

static void select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, AppData *data) {
  (void)menu_layer;
  (void)data;
  switch (cell_index->row) {
    case 1: {
      int num_beers = persist_read_int(COUNT_PKEY);
      persist_write_int(COUNT_PKEY, num_beers + (500 + rand() % 500));
      menu_layer_reload_data(menu_layer);
      break;
    }
    case 2: {
      int num_beers = persist_read_int(COUNT_PKEY);
      persist_write_int(COUNT_PKEY, num_beers - (500 + rand() % 500));
      menu_layer_reload_data(menu_layer);
    }
  }
}

static void prv_window_load(Window *window) {
  AppData *data = window_get_user_data(window);

  MenuLayer *menu_layer = &data->menu_layer;
  menu_layer_init(menu_layer, &window->layer.bounds);
  menu_layer_set_callbacks(menu_layer, data, &(MenuLayerCallbacks) {
    .get_num_sections = (MenuLayerGetNumberOfSectionsCallback) get_num_sections_callback,
    .get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
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

static void handle_init() {
  AppData *data = (AppData *)app_zalloc_check(sizeof(AppData));
  app_state_set_user_data(data);
  push_window(data);

  const int exist_result = persist_exists(COUNT_PKEY);
  PBL_LOG(LOG_LEVEL_DEBUG, "- exist_result %d", exist_result);
  if (exist_result == false) {
    PBL_LOG(LOG_LEVEL_DEBUG, "- writing...");
    persist_write_int(COUNT_PKEY, 10);
  }
}

static void handle_deinit() {
  AppData *data = app_state_get_user_data();
  menu_layer_deinit(&data->menu_layer);
  app_free(data);
}

static void s_main() {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* persist_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Persist Demo"
  };
  return (const PebbleProcessMd*) &s_app_info;
}

#undef BUFFER_SIZE
