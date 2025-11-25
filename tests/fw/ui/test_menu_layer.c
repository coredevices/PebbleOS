/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "applib/ui/menu_layer.h"
#include "applib/ui/content_indicator_private.h"

// Stubs
/////////////////////
#include "stubs_app_state.h"
#include "stubs_graphics.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_ui_window.h"
#include "stubs_process_manager.h"
#include "stubs_unobstructed_area.h"


// Fakes
////////////////////////

//#include "fake_gbitmap_png.c"

GDrawState graphics_context_get_drawing_state(GContext* ctx) {
  return (GDrawState){};
}

void graphics_context_set_drawing_state(GContext* ctx, GDrawState draw_state) {}
void graphics_context_set_fill_color(GContext* ctx, GColor color){}

Layer* inverter_layer_get_layer(InverterLayer *inverter_layer) {
  return &inverter_layer->layer;
}

void inverter_layer_init(InverterLayer *inverter, const GRect *frame) {}

void window_long_click_subscribe(ButtonId button_id, uint16_t delay_ms,
                                 ClickHandler down_handler, ClickHandler up_handler) {}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {}
void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms,
                                             ClickHandler handler) {}
void window_set_click_config_provider_with_context(Window *window,
                                                   ClickConfigProvider click_config_provider,
                                                   void *context) {}
void window_set_click_context(ButtonId button_id, void *context) {}

void content_indicator_destroy_for_scroll_layer(ScrollLayer *scroll_layer) {}

ContentIndicator s_content_indicator;
ContentIndicator *content_indicator_get_for_scroll_layer(ScrollLayer *scroll_layer) {
  return &s_content_indicator;
}
ContentIndicator *content_indicator_get_or_create_for_scroll_layer(ScrollLayer *scroll_layer) {
  return &s_content_indicator;
}

static bool s_content_available[NumContentIndicatorDirections];
void content_indicator_set_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction,
                                             bool available) {
  s_content_available[direction] = available;
}

void graphics_context_set_compositing_mode(GContext* ctx, GCompOp mode) {}
void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect){}

int16_t menu_cell_basic_cell_height(void) {
  return 44;
}

// Tests
//////////////////////


static uint16_t s_num_rows;

void test_menu_layer__initialize(void) {
  s_num_rows = 10;
}

void test_menu_layer__cleanup(void) {
}

static void prv_draw_row(GContext* ctx,
                         const Layer *cell_layer,
                         MenuIndex *cell_index,
                         void *callback_context) {}

static uint16_t prv_get_num_rows(struct MenuLayer *menu_layer,
                                 uint16_t section_index,
                                 void *callback_context) {
  return s_num_rows;
}

void test_menu_layer__test_set_selected_classic(void) {
  MenuLayer l;
  menu_layer_init(&l, &GRect(10, 10, 180, 180));
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks){
      .draw_row = prv_draw_row,
      .get_num_rows = prv_get_num_rows,
  });
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);
  cl_assert_equal_i(0, scroll_layer_get_content_offset(&l.scroll_layer).y);

  menu_layer_set_selected_index(&l, MenuIndex(0, 1), MenuRowAlignTop, false);
  cl_assert_equal_i(1, menu_layer_get_selected_index(&l).row);
  const int16_t basic_cell_height = menu_cell_basic_cell_height();
  cl_assert_equal_i(basic_cell_height, l.selection.y);
  cl_assert_equal_i(-basic_cell_height,
                    scroll_layer_get_content_offset(&l.scroll_layer).y);
}

void test_menu_layer__test_set_selected_center_focused(void) {
  MenuLayer l;
  const int height = 180;
  menu_layer_init(&l, &GRect(10, 10, height, 180));
  menu_layer_set_center_focused(&l, true);
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks){
      .draw_row = prv_draw_row,
      .get_num_rows = prv_get_num_rows,
  });
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);
  const int16_t basic_cell_height = menu_cell_basic_cell_height();
  const int row0_vertically_centered = (height - basic_cell_height)/2;
  cl_assert_equal_i(row0_vertically_centered, scroll_layer_get_content_offset(&l.scroll_layer).y);

  menu_layer_set_selected_index(&l, MenuIndex(0, 1), MenuRowAlignTop, false);
  cl_assert_equal_i(1, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(basic_cell_height, l.selection.y);

  const int y_center_of_row_1 = basic_cell_height + basic_cell_height / 2;
  const int row1_vertically_centered = height / 2 - y_center_of_row_1;
  cl_assert_equal_i(row1_vertically_centered, scroll_layer_get_content_offset(&l.scroll_layer).y);
}

void test_menu_layer__test_set_selection_animation(void) {
  MenuLayer l;
  const int height = 180;
  menu_layer_init(&l, &GRect(10, 10, height, 180));
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks){
    .draw_row = prv_draw_row,
    .get_num_rows = prv_get_num_rows,
  });
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);

  // Test disabled first
  l.selection_animation_disabled = true;
  menu_layer_set_selected_index(&l, MenuIndex(0, 1), MenuRowAlignTop, true);
  cl_assert_equal_i(1, menu_layer_get_selected_index(&l).row);
  cl_assert(!l.animation.animation);

  // Test enabled
  l.selection_animation_disabled = false;
  menu_layer_set_selected_index(&l, MenuIndex(0, 0), MenuRowAlignTop, true);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert(l.animation.animation);
}

int16_t prv_get_row_height_depending_on_selection_state(struct MenuLayer *menu_layer,
                                                        MenuIndex *cell_index,
                                                        void *callback_context) {
  MenuIndex selected_index = menu_layer_get_selected_index(menu_layer);
  bool is_selected = menu_index_compare(&selected_index, cell_index) == 0;
  return is_selected ? MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT : menu_cell_basic_cell_height();
}

void test_menu_layer__default_ignores_row_height_for_selection(void) {
  MenuLayer l;
  const int height = 180;
  menu_layer_init(&l, &GRect(10, 10, height, 180));
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks){
      .draw_row = prv_draw_row,
      .get_num_rows = prv_get_num_rows,
      .get_cell_height = prv_get_row_height_depending_on_selection_state,
  });
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);
  cl_assert_equal_i(0, scroll_layer_get_content_offset(&l.scroll_layer).y);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  const int FOCUSED = MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT;
  const int NORMAL = menu_cell_basic_cell_height();

  cl_assert_equal_i(FOCUSED, l.selection.h);

  menu_layer_set_selected_index(&l, MenuIndex(0, 2), MenuRowAlignNone, false);

  cl_assert(menu_layer_get_center_focused(&l) == false);
  // non-center-focus behavior: don't ask adjust for changed height of row(0,0)
  cl_assert_equal_i(FOCUSED + 1 * NORMAL, l.selection.y);
  // also non-center-focus behavior: don't update selected_index before asking row (0,1) for height
  cl_assert_equal_i(NORMAL, l.selection.h);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  // in general, the default behavior does not handle changes in row height correctly
  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, false);
  cl_assert_equal_i(2 * FOCUSED + NORMAL, l.selection.y);
  cl_assert_equal_i(NORMAL, l.selection.h);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  // totally wrong
  menu_layer_set_selected_next(&l, true, MenuRowAlignNone, false);
  cl_assert_equal_i(2 * FOCUSED, l.selection.y);
  cl_assert_equal_i(NORMAL, l.selection.h);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  // WTF?!
  menu_layer_set_selected_index(&l, MenuIndex(0, 1), MenuRowAlignNone, false);
  cl_assert_equal_i(2 * FOCUSED - NORMAL, l.selection.y);
  cl_assert_equal_i(NORMAL, l.selection.h);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);
}

void test_menu_layer__center_focused_respects_row_height_for_selection(void) {
  MenuLayer l;
  const int height = 180;
  menu_layer_init(&l, &GRect(10, 10, height, 180));
  menu_layer_set_center_focused(&l, true);
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks){
      .draw_row = prv_draw_row,
      .get_num_rows = prv_get_num_rows,
      .get_cell_height = prv_get_row_height_depending_on_selection_state,
  });

  const int FOCUSED = MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT;
  const int NORMAL = menu_cell_basic_cell_height();

  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);
  const int row0_vertically_centered = (height - FOCUSED)/2;
  cl_assert_equal_i(row0_vertically_centered, scroll_layer_get_content_offset(&l.scroll_layer).y);
  cl_assert_equal_i(FOCUSED, l.selection.h);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  menu_layer_set_selected_index(&l, MenuIndex(0, 2), MenuRowAlignNone, false);
  // new center-focus behavior: adjust for changed row sizes depending on focused row
  cl_assert(menu_layer_get_center_focused(&l) == true);
  cl_assert_equal_i(2 * NORMAL, l.selection.y);
  cl_assert_equal_i(NORMAL - FOCUSED, scroll_layer_get_content_offset(&l.scroll_layer).y);
  cl_assert_equal_i(FOCUSED, l.selection.h);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, false);
  cl_assert_equal_i(3 * NORMAL, l.selection.y);
  cl_assert_equal_i(-FOCUSED, scroll_layer_get_content_offset(&l.scroll_layer).y);
  cl_assert_equal_i(FOCUSED, l.selection.h);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  menu_layer_set_selected_next(&l, true, MenuRowAlignNone, false);
  cl_assert_equal_i(2 * NORMAL, l.selection.y);
  cl_assert_equal_i(NORMAL - FOCUSED, scroll_layer_get_content_offset(&l.scroll_layer).y);
  cl_assert_equal_i(FOCUSED, l.selection.h);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);

  menu_layer_set_selected_index(&l, MenuIndex(0, 1), MenuRowAlignNone, false);
  cl_assert_equal_i(1 * NORMAL, l.selection.y);
  cl_assert_equal_i(2 * NORMAL - FOCUSED, scroll_layer_get_content_offset(&l.scroll_layer).y);
  cl_assert_equal_i(FOCUSED, l.selection.h);
  cl_assert_equal_b(false, s_content_available[ContentIndicatorDirectionUp]);
  cl_assert_equal_b(true, s_content_available[ContentIndicatorDirectionDown]);
}

static void prv_skip_odd_rows(struct MenuLayer *menu_layer,
                               MenuIndex *new_index,
                               MenuIndex old_index,
                               void *callback_context) {
  if (new_index->row == 1) {
    new_index->row = 2;
  }
  if (new_index->row == 3) {
    new_index->row = 4;
  }
}

void test_menu_layer__center_focused_handles_skipped_rows(void) {
  MenuLayer l;
  menu_layer_init(&l, &GRect(10, 10, DISP_COLS, DISP_ROWS));
  menu_layer_set_center_focused(&l, true);
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks) {
    .draw_row = prv_draw_row,
    .get_num_rows = prv_get_num_rows,
    .selection_will_change = prv_skip_odd_rows,
  });
  menu_layer_reload_data(&l);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);

  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, false);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  cl_assert_equal_i(2, menu_layer_get_selected_index(&l).row);
  const int16_t basic_cell_height = menu_cell_basic_cell_height();
  cl_assert_equal_i(2 * basic_cell_height, l.selection.y);

  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, false);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  cl_assert_equal_i(4, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(4 * basic_cell_height, l.selection.y);

  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, false);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  cl_assert_equal_i(5, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(5 * basic_cell_height, l.selection.y);

  menu_layer_set_selected_next(&l, true, MenuRowAlignNone, false);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  cl_assert_equal_i(4, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(4 * basic_cell_height, l.selection.y);
}

void test_menu_layer__center_focused_handles_skipped_rows_animated(void) {
  MenuLayer l;
  menu_layer_init(&l, &GRect(10, 10, DISP_COLS, DISP_ROWS));
  menu_layer_set_center_focused(&l, true);
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks) {
    .draw_row = prv_draw_row,
    .get_num_rows = prv_get_num_rows,
    .selection_will_change = prv_skip_odd_rows,
  });
  menu_layer_reload_data(&l);
  const int16_t basic_cell_height = menu_cell_basic_cell_height();
  const int initial_scroll_offset = (DISP_ROWS - basic_cell_height) / 2;
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0, l.selection.y);
  cl_assert_equal_i(initial_scroll_offset, l.scroll_layer.content_sublayer.bounds.origin.y);

  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, true);
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).section);
  // these values are unchanged until the animation updates them
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0 * basic_cell_height, l.selection.y);
  cl_assert_equal_i(initial_scroll_offset, l.scroll_layer.content_sublayer.bounds.origin.y);

  // in this test setup, we can directly cast an animation to AnimationPrivate
  AnimationPrivate *ap = (AnimationPrivate *) l.animation.animation;
  const AnimationImplementation *const impl = ap->implementation;
  impl->update(l.animation.animation, ANIMATION_NORMALIZED_MAX / 10);
  // still unchanged
  cl_assert_equal_i(0, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(0 * basic_cell_height, l.selection.y);
  cl_assert_equal_i(initial_scroll_offset, l.scroll_layer.content_sublayer.bounds.origin.y);

  // and updated
  impl->update(l.animation.animation, ANIMATION_NORMALIZED_MAX * 9 / 10);
  cl_assert_equal_i(2, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(2 * basic_cell_height, l.selection.y);
  cl_assert_equal_i(initial_scroll_offset - 2 * basic_cell_height,
                    l.scroll_layer.content_sublayer.bounds.origin.y);

  animation_unschedule(l.animation.animation);
  menu_layer_set_selected_next(&l, false, MenuRowAlignNone, true);
  // these values are unchanged until the animation updates them
  cl_assert_equal_i(2, menu_layer_get_selected_index(&l).row);
  cl_assert_equal_i(2 * basic_cell_height, l.selection.y);
  cl_assert_equal_i(initial_scroll_offset - 2 * basic_cell_height,
                    l.scroll_layer.content_sublayer.bounds.origin.y);
}

static MenuLayer s_menu_layer_hierarchy;

static void prv_menu_cell_is_part_of_hierarchy_draw_row(GContext* ctx,
                                                        const Layer *cell_layer,
                                                        MenuIndex *cell_index,
                                                        void *callback_context) {
  cl_assert_equal_p(cell_layer->window, s_menu_layer_hierarchy.scroll_layer.layer.window);
  cl_assert_equal_p(cell_layer->parent, &s_menu_layer_hierarchy.scroll_layer.content_sublayer);
  const GPoint actual = layer_convert_point_to_screen(cell_layer, GPointZero);
  const GPoint expected = layer_convert_point_to_screen(&s_menu_layer_hierarchy.scroll_layer.layer,
                                                        GPoint(0, cell_index->row * 44));
  cl_assert_equal_gpoint(actual, expected);
}

int prv_num_sublayers(const Layer *l) {
  int result = 0;
  Layer *child = l->first_child;
  while (l) {
    l = l->next_sibling;
    result++;
  }
  return result;
}

void test_menu_layer__menu_cell_is_part_of_hierarchy(void) {
  menu_layer_init(&s_menu_layer_hierarchy, &GRect(10, 10, 100, 180));
  Layer *layer = &s_menu_layer_hierarchy.scroll_layer.content_sublayer;
  // two layers (inverter + shadow)
  cl_assert_equal_i(2, prv_num_sublayers(layer));
  menu_layer_set_callbacks(&s_menu_layer_hierarchy, NULL, &(MenuLayerCallbacks){
    .draw_row = prv_menu_cell_is_part_of_hierarchy_draw_row,
    .get_num_rows = prv_get_num_rows,
  });
  menu_layer_reload_data(&s_menu_layer_hierarchy);
  GContext ctx = {};
  cl_assert_equal_i(2, prv_num_sublayers(layer));
  layer->update_proc(layer, &ctx);
  cl_assert_equal_i(2, prv_num_sublayers(layer));
}

void test_menu_layer__center_focused_updates_height_on_reload(void) {
  MenuLayer l;
  const int height = DISP_ROWS;
  menu_layer_init(&l, &GRect(10, 10, height, DISP_COLS));
  menu_layer_set_center_focused(&l, true);
  s_num_rows = 3;
  menu_layer_set_callbacks(&l, NULL, &(MenuLayerCallbacks) {
    .draw_row = prv_draw_row,
    .get_num_rows = prv_get_num_rows,
    .get_cell_height = prv_get_row_height_depending_on_selection_state,
  });
  menu_layer_set_center_focused(&l, true);
  menu_layer_reload_data(&l);
  const int focused_height = MENU_CELL_ROUND_FOCUSED_TALL_CELL_HEIGHT;

  // focus last row
  menu_layer_set_selected_index(&l, MenuIndex(0, s_num_rows - 1), MenuRowAlignNone, false);
  cl_assert_equal_i(focused_height, l.selection.h);

  s_num_rows--;
  cl_assert_equal_i(2, s_num_rows);
  menu_layer_reload_data(&l);
  cl_assert_equal_i(s_num_rows - 1, l.selection.index.row);
  cl_assert_equal_i(focused_height, l.selection.h);

  s_num_rows--;
  cl_assert_equal_i(1, s_num_rows);
  menu_layer_reload_data(&l);
  cl_assert_equal_i(s_num_rows - 1, l.selection.index.row);
  cl_assert_equal_i(focused_height, l.selection.h);
}
