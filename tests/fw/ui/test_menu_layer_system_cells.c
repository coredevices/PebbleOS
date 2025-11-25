/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/status_bar_layer.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "util/graphics.h"
#include "util/size.h"

#include "clar.h"

#include <stdio.h>

// Fakes
/////////////////////

#include "fake_resource_syscalls.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

static bool s_cell_is_highlighted = false;

// TODO PBL-23041: When round MenuLayer animations are enabled, we need a "is_selected" function
bool menu_cell_layer_is_highlighted(const Layer *cell_layer) {
  return s_cell_is_highlighted;
}

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_app_state.h"
#include "stubs_bootbits.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_content_indicator.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"

void window_long_click_subscribe(ButtonId button_id, uint16_t delay_ms, ClickHandler down_handler,
                                 ClickHandler up_handler) {}

void window_set_click_config_provider_with_context(Window *window,
                                                   ClickConfigProvider click_config_provider,
                                                   void *context) {}

void window_set_click_context(ButtonId button_id, void *context) {}

void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {}

void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms,
                                             ClickHandler handler) {}

// Helper Functions
/////////////////////
#include "../graphics/test_graphics.h"
#include "../graphics/util.h"

// Setup and Teardown
////////////////////////////////////

static FrameBuffer *fb;
static GContext s_ctx;

static GBitmap *s_dest_bitmap;

static GBitmap s_tictoc_icon_bitmap;
static GBitmap s_smart_alarm_icon_bitmap;

static void prv_initialize_icons(void) {
  gbitmap_init_with_resource(&s_tictoc_icon_bitmap, RESOURCE_ID_MENU_ICON_TICTOC_WATCH);
  gbitmap_init_with_resource(&s_smart_alarm_icon_bitmap, RESOURCE_ID_SMART_ALARM_ICON_BLACK);
}

void test_menu_layer_system_cells__initialize(void) {
  process_manager_set_compiled_with_legacy2_sdk(false);

  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});

  test_graphics_context_init(&s_ctx, fb);
  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false /* is_next */);

  resource_init();

  prv_initialize_icons();
}

void test_menu_layer_system_cells__cleanup(void) {
  free(fb);
  fb = NULL;

  gbitmap_deinit(&s_tictoc_icon_bitmap);
  gbitmap_deinit(&s_smart_alarm_icon_bitmap);

  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = NULL;
}

// Helpers
//////////////////////

// This struct represents the data for a column of the grid of our resulting test image
typedef struct {
  const char *title_font;
  const char *subtitle_font;
  const char *value_font;
} MenuLayerSystemCellTestColumnData;

static const MenuLayerSystemCellTestColumnData s_menu_system_basic_cell_test_column_data[] = {
  { NULL, NULL, NULL }, // Use the default fonts
};

static const MenuLayerSystemCellTestColumnData s_menu_system_cell_layer_test_column_data[] = {
  { FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_14, FONT_KEY_GOTHIC_24_BOLD },
  { FONT_KEY_GOTHIC_14, FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_14 },
};

// This struct represents the data for a row of the grid of our resulting test image
typedef struct {
  char *title;
  char *subtitle;
  char *value;
  GBitmap *icon;
  MenuCellLayerIconAlign icon_align;
  bool icon_subbitmap;
  GBoxModel *icon_box_model;
  int horizontal_inset;
  bool icon_form_fit;
} MenuLayerSystemCellTestRowData;

#define DEFAULT_ICON_ALIGN PBL_IF_RECT_ELSE(MenuCellLayerIconAlign_Left, MenuCellLayerIconAlign_Top)

static const MenuLayerSystemCellTestRowData s_menu_system_cell_test_row_data[] = {
  {"Star Wars", NULL, NULL, NULL, DEFAULT_ICON_ALIGN},
  {"The Lord of the Rings", "The Fellowship of the Ring", NULL, NULL, DEFAULT_ICON_ALIGN},
  {"The Lord of the Rings", NULL, NULL, &s_tictoc_icon_bitmap, DEFAULT_ICON_ALIGN},
  {"The Matrix", "Revolutions", NULL, &s_tictoc_icon_bitmap, DEFAULT_ICON_ALIGN},
  {"8:00 AM", "Weekdays", "OFF", NULL, DEFAULT_ICON_ALIGN},
  {"8:00 AM", "Weekdays", NULL, &s_tictoc_icon_bitmap, MenuCellLayerIconAlign_Right},
  {"8:00 AM", "Weekdays", "OFF", &s_smart_alarm_icon_bitmap, MenuCellLayerIconAlign_TopLeft,
   false, &(GBoxModel){ .offset = { 0, 5 }, .margin = { 6, 0 }}, PBL_IF_ROUND_ELSE(-6, 0), true},
  {"The Lord of the Rings", NULL, NULL, &s_tictoc_icon_bitmap, DEFAULT_ICON_ALIGN, true},
  {"The Matrix", "Revolutions", NULL, &s_tictoc_icon_bitmap, DEFAULT_ICON_ALIGN, true},
};

// We render all of the row data's for each of the following heights
static const int16_t s_menu_system_cell_test_row_heights[] = {
#if PBL_ROUND
  MENU_CELL_ROUND_UNFOCUSED_SHORT_CELL_HEIGHT,
  MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT,
#endif
  0, //!< Will be interpreted as "use menu_cell_basic_cell_height()"
  MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT,
  MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT,
};

#define GRID_CELL_PADDING 5

static int16_t prv_get_row_height_for_index(int i) {
  return (s_menu_system_cell_test_row_heights[i] == 0) ? menu_cell_basic_cell_height() :
                                                         s_menu_system_cell_test_row_heights[i];
}

static int16_t prv_calculate_overall_grid_height(void) {
  int16_t sum = 0;
  const unsigned int num_distinct_rows = ARRAY_LENGTH(s_menu_system_cell_test_row_data);
  const unsigned int num_row_heights = ARRAY_LENGTH(s_menu_system_cell_test_row_heights);
  for (int i = 0; i < num_row_heights; i++) {
    sum += GRID_CELL_PADDING + prv_get_row_height_for_index(i);
  }
  return (sum * num_distinct_rows) + GRID_CELL_PADDING;
}

typedef enum MenuCellType {
  MenuCellType_Basic,
  MenuCellType_BasicCustom,
  MenuCellType_CellLayer,
} MenuCellType;

//! Allows testing other cell drawing functions using MenuCellLayerConfig
static void prv_menu_cell_draw_dispatch(GContext *ctx, Layer *cell_layer, MenuCellType cell_type,
                                        const MenuCellLayerConfig *config, bool icon_subbitmap) {
  GRect old_icon_bounds = GRectZero;
  if (config->icon) {
    old_icon_bounds = config->icon->bounds;
    if (icon_subbitmap) {
      gpoint_add_eq(&config->icon->bounds.origin, GPoint(4, 4));
      config->icon->bounds.size.w -= 8;
      config->icon->bounds.size.h -= 8;
    }
  }
  switch (cell_type) {
    case MenuCellType_Basic:
      // These should be ignored, we want to make sure they are!
      menu_cell_basic_draw(ctx, cell_layer, config->title, config->subtitle, config->icon);
      break;
    case MenuCellType_BasicCustom:
      menu_cell_basic_draw_custom(
          ctx, cell_layer, config->title_font, config->title, config->value_font, config->value,
          config->subtitle_font, config->subtitle, config->icon,
          (config->icon_align == MenuCellLayerIconAlign_Right), config->overflow_mode);
      break;
    case MenuCellType_CellLayer:
      menu_cell_layer_draw(ctx, cell_layer, config);
      break;
  }

  if (config->icon) {
    config->icon->bounds = old_icon_bounds;
  }
}

static void prv_draw_cell(MenuCellType cell_type, const GRect *cell_bounds,
                          const MenuLayerSystemCellTestRowData *row_data,
                          const MenuLayerSystemCellTestColumnData *column_data, bool is_selected) {
  s_ctx.draw_state.drawing_box.origin = cell_bounds->origin;
  const GRect cell_frame = GRect(0, 0, cell_bounds->size.w, cell_bounds->size.h);

  const GColor background_color = is_selected ? GColorCobaltBlue : GColorWhite;
  graphics_context_set_fill_color(&s_ctx, background_color);
  graphics_fill_rect(&s_ctx, &cell_frame);

  const GColor foreground_color = is_selected ? GColorWhite : GColorBlack;
  graphics_context_set_text_color(&s_ctx, foreground_color);
  graphics_context_set_tint_color(&s_ctx, foreground_color);
  graphics_context_set_stroke_color(&s_ctx, foreground_color);

  Layer cell_layer;
  layer_init(&cell_layer, &cell_frame);
  cell_layer.is_highlighted = is_selected;
  s_cell_is_highlighted = is_selected;

  const MenuCellLayerConfig config = {
    .title_font = column_data->title_font ? fonts_get_system_font(column_data->title_font) : NULL,
    .subtitle_font = column_data->subtitle_font ? fonts_get_system_font(column_data->subtitle_font) : NULL,
    .value_font = column_data->value_font ? fonts_get_system_font(column_data->value_font) : NULL,
    .title = row_data->title,
    .subtitle = row_data->subtitle,
    .value = row_data->value,
    .icon = row_data->icon,
    .icon_align = row_data->icon_align,
    .icon_box_model = row_data->icon_box_model,
    .icon_form_fit = row_data->icon_form_fit,
    .horizontal_inset = row_data->horizontal_inset,
    .overflow_mode = GTextOverflowModeFill,
  };
  prv_menu_cell_draw_dispatch(&s_ctx, &cell_layer, cell_type, &config, row_data->icon_subbitmap);
}

void prv_prepare_canvas_and_render_cells(MenuCellType cell_type, int16_t cell_width,
                                         const MenuLayerSystemCellTestColumnData *columns,
                                         unsigned int num_columns, bool is_legacy2) {
  gbitmap_destroy(s_dest_bitmap);

  const unsigned int num_row_heights = ARRAY_LENGTH(s_menu_system_cell_test_row_heights);
  const unsigned int num_rows = ARRAY_LENGTH(s_menu_system_cell_test_row_data);

  // Multiply num_columns * 2 to account for drawing both focused and unfocused side-by-side
  const int num_columns_accounting_for_focused_unfocused = num_columns * 2;
  const int16_t bitmap_width = (cell_width * num_columns_accounting_for_focused_unfocused) +
    (GRID_CELL_PADDING * (num_columns_accounting_for_focused_unfocused + 1));
  const int16_t bitmap_height = prv_calculate_overall_grid_height();
  const GSize bitmap_size = GSize(bitmap_width, bitmap_height);
  s_dest_bitmap = gbitmap_create_blank(bitmap_size,
                                       PBL_IF_COLOR_ELSE(GBitmapFormat8Bit, GBitmapFormat1Bit));

  s_ctx.dest_bitmap = *s_dest_bitmap;
  s_ctx.draw_state.clip_box.size = bitmap_size;
  s_ctx.draw_state.drawing_box.size = bitmap_size;

  // Fill the bitmap with pink so it's easier to see errors
  memset(s_dest_bitmap->addr, GColorShockingPinkARGB8,
         s_dest_bitmap->row_size_bytes * s_dest_bitmap->bounds.size.h);

  process_manager_set_compiled_with_legacy2_sdk(is_legacy2);

  int16_t y_offset = 0;
  for (unsigned int row_index = 0; row_index < num_rows; row_index++) {
    const MenuLayerSystemCellTestRowData row_data = 
        s_menu_system_cell_test_row_data[row_index];
    for (unsigned int row_height_index = 0; row_height_index < num_row_heights;
         row_height_index++) {
      y_offset += GRID_CELL_PADDING;
      const int16_t row_height = prv_get_row_height_for_index(row_height_index);
      for (unsigned int column_index = 0; column_index < num_columns; column_index++) {
        const MenuLayerSystemCellTestColumnData column_data = columns[column_index];

        int16_t x_offset = GRID_CELL_PADDING + column_index * ((GRID_CELL_PADDING + cell_width) * 2);

        GRect cell_bounds = GRect(x_offset, y_offset, cell_width, row_height);
        prv_draw_cell(cell_type, &cell_bounds, &row_data, &column_data, true);
        cell_bounds.origin.x += cell_width + GRID_CELL_PADDING;
        prv_draw_cell(cell_type, &cell_bounds, &row_data, &column_data, false);
      }
      y_offset += row_height;
    }
  }

  process_manager_set_compiled_with_legacy2_sdk(false);
}

// Tests
//////////////////////

void test_menu_layer_system_cells__basic_cell_width_144_legacy2(void) {
#if PLATFORM_TINTIN || PLATFORM_SILK || PLATFORM_ASTERIX
  // NOTE: The generated bitmap will look really funky because it's rendering 8bit gbitmaps as
  //       1bit due to the legacy2 check in gbitmap_get_format. This is normal and expected.

  const int16_t cell_width = 144;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_Basic, cell_width,
      s_menu_system_basic_cell_test_column_data,
      ARRAY_LENGTH(s_menu_system_basic_cell_test_column_data),
      /* is_legacy2 */ true);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_menu_layer_system_cells__basic_cell_width_144(void) {
  const int16_t cell_width = 144;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_Basic, cell_width,
      s_menu_system_basic_cell_test_column_data,
      ARRAY_LENGTH(s_menu_system_basic_cell_test_column_data),
      /* is_legacy2 */ false);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_menu_layer_system_cells__basic_custom_cell_width_144(void) {
  const int16_t cell_width = 144;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_BasicCustom, cell_width,
      s_menu_system_cell_layer_test_column_data,
      ARRAY_LENGTH(s_menu_system_cell_layer_test_column_data),
      /* is_legacy2 */ false);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_menu_layer_system_cells__cell_width_32(void) {
#if PBL_ROUND
  const int16_t cell_width = 32;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_CellLayer, cell_width,
      s_menu_system_cell_layer_test_column_data,
      ARRAY_LENGTH(s_menu_system_cell_layer_test_column_data),
      /* is_legacy2 */ false);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_menu_layer_system_cells__cell_width_100(void) {
  const int16_t cell_width = 100;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_CellLayer, cell_width,
      s_menu_system_cell_layer_test_column_data,
      ARRAY_LENGTH(s_menu_system_cell_layer_test_column_data),
      /* is_legacy2 */ false);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_menu_layer_system_cells__cell_width_144(void) {
  const int16_t cell_width = 144;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_CellLayer, cell_width,
      s_menu_system_cell_layer_test_column_data,
      ARRAY_LENGTH(s_menu_system_cell_layer_test_column_data),
      /* is_legacy2 */ false);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_menu_layer_system_cells__cell_width_180(void) {
  const int16_t cell_width = 180;
  prv_prepare_canvas_and_render_cells(
      MenuCellType_CellLayer, cell_width,
      s_menu_system_cell_layer_test_column_data,
      ARRAY_LENGTH(s_menu_system_cell_layer_test_column_data),
      /* is_legacy2 */ false);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}
