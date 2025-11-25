/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "applib/ui/text_layer_flow.h"
#include "applib/ui/scroll_layer.h"

// Stubs
/////////////////////
#include "stubs_app_state.h"
#include "stubs_fonts.h"
#include "stubs_graphics.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_ui_window.h"
#include "stubs_process_manager.h"
#include "stubs_system_theme.h"
#include "stubs_text_layout.h"
#include "stubs_unobstructed_area.h"

void graphics_context_set_fill_color(GContext* ctx, GColor color){}
void graphics_context_set_text_color(GContext* ctx, GColor color){}

// Fakes
////////////////////////

Layer *scroll_layer_is_instance_value;
bool scroll_layer_is_instance(const Layer *layer) {
  return scroll_layer_is_instance_value == layer;
}

TextLayer text_layer;
Window window;

void test_text_layer_flow__initialize(void) {
  window = (Window){.layer.window = &window};
  text_layer_init(&text_layer, &GRect(10, 20, 30, 40));
  text_layer.layer.window = &window;
  scroll_layer_is_instance_value = NULL;
}

void test_text_layer_flow__cleanup(void) {
}

void test_text_layer_flow__return_value_handling(void) {
  GPoint origin;
  GRect page;
  cl_assert(false == text_layer_calc_text_flow_paging_values(&text_layer, &origin, &page));
  layer_add_child(&window.layer, &text_layer.layer);
  cl_assert(true == text_layer_calc_text_flow_paging_values(&text_layer, &origin, &page));
  cl_assert(true == text_layer_calc_text_flow_paging_values(&text_layer, &origin, NULL));
  cl_assert(true == text_layer_calc_text_flow_paging_values(&text_layer, NULL, &page));
  cl_assert(true == text_layer_calc_text_flow_paging_values(&text_layer, NULL, NULL));
  cl_assert(false == text_layer_calc_text_flow_paging_values(NULL, NULL, NULL));

  cl_assert_equal_gpoint(origin, text_layer.layer.frame.origin);
  cl_assert_equal_gpoint(page.origin, origin);
  cl_assert_equal_gsize(page.size, GSize(text_layer.layer.frame.size.w,
                                        TEXT_LAYER_FLOW_DEFAULT_PAGING_HEIGHT));
}

void test_text_layer_flow__paging_container(void) {
  Layer container;
  layer_init(&container, &GRect(30, 40, 100, 10));
  layer_add_child(&window.layer, &container);
  layer_add_child(&container, &text_layer.layer);

  GPoint origin;
  GRect page;
  scroll_layer_is_instance_value = NULL;
  cl_assert(true == text_layer_calc_text_flow_paging_values(&text_layer, &origin, &page));

  // text_layer's absolute coordinate as
  cl_assert_equal_gpoint(origin, GPoint(40, 60));
  // text_layer's absolute coordinate as there's no container
  cl_assert_equal_gpoint(page.origin, GPoint(40, 60));
  cl_assert_equal_gsize(page.size, GSize(30, TEXT_LAYER_FLOW_DEFAULT_PAGING_HEIGHT));

  scroll_layer_is_instance_value = &container;
  cl_assert(true == text_layer_calc_text_flow_paging_values(&text_layer, &origin, &page));

  cl_assert_equal_gpoint(origin, GPoint(40, 60));
  // containers's absolute coordinate
  cl_assert_equal_gpoint(page.origin, GPoint(30, 40));
  cl_assert_equal_gsize(page.size, GSize(100, 10));
}

void test_text_layer_flow__no_overflow_on_default_page_height(void) {
  // first, make sure that grect_max_y itself overflows
  cl_assert(grect_get_max_y(&(GRect){.origin.y = 1, .size.h = INT16_MAX}) < 0);

  text_layer.layer.frame.origin.y = 1;
  layer_add_child(&window.layer, &text_layer.layer);

  GPoint origin;
  GRect page;
  cl_assert(text_layer_calc_text_flow_paging_values(&text_layer, &origin, &page));
  cl_assert_equal_i(page.origin.y, text_layer.layer.frame.origin.y);

  // must not overflow
  cl_assert(grect_get_max_y(&page) > 0);
}
