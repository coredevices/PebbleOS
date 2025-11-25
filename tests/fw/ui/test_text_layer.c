/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/text_layer.h"

// Stubs
/////////////////////
#include "stubs_app_state.h"
#include "stubs_graphics.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_system_theme.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"

GFont fonts_get_system_font(const char *font_key) {
  return NULL;
}

void graphics_context_set_fill_color(GContext* ctx, GColor color){}
void graphics_context_set_text_color(GContext* ctx, GColor color){}

// Fakes
////////////////////////


#define MOCKED_CREATED_LAYOUT (GTextLayoutCacheRef)123456
void graphics_text_layout_cache_init(GTextLayoutCacheRef *layout_cache) {
  *layout_cache = MOCKED_CREATED_LAYOUT;
}

void graphics_text_layout_cache_deinit(GTextLayoutCacheRef *layout_cache) {}
void graphics_text_layout_set_line_spacing_delta(GTextLayoutCacheRef layout, int16_t delta) {}

int16_t graphics_text_layout_get_line_spacing_delta(const GTextLayoutCacheRef layout) {
  return 0;
}

GSize graphics_text_layout_get_max_used_size(GContext *ctx, const char *text,
                                             GFont const font, const GRect box,
                                             const GTextOverflowMode overflow_mode,
                                             const GTextAlignment alignment,
                                             GTextLayoutCacheRef layout) {
  return GSizeZero;
}

typedef struct {
  struct {
    GTextLayoutCacheRef layout;
  } disable_text_flow;
  struct {
    GTextLayoutCacheRef layout;
    uint8_t inset;
  } enable_text_flow;
  struct {
    GTextLayoutCacheRef layout;
  } disable_paging;
  struct {
    GTextLayoutCacheRef layout;
    GPoint origin;
    GRect paging;
  } enable_paging;
} MockValues;

static MockValues s_actual;

void graphics_text_attributes_restore_default_text_flow(GTextLayoutCacheRef layout) {
  s_actual.disable_text_flow.layout = layout;
}

void graphics_text_attributes_enable_screen_text_flow(GTextLayoutCacheRef layout, uint8_t inset) {
  s_actual.enable_text_flow.layout = layout;
  s_actual.enable_text_flow.inset = inset;
}

void graphics_text_attributes_restore_default_paging(GTextLayoutCacheRef layout) {
  s_actual.disable_paging.layout = layout;
}

void graphics_text_attributes_enable_paging(GTextLayoutCacheRef layout,
                                            GPoint content_origin_on_screen, GRect paging_on_screen) {
  s_actual.enable_paging.layout = layout;
  s_actual.enable_paging.origin = content_origin_on_screen;
  s_actual.enable_paging.paging = paging_on_screen;
}

#define MOCKED_PAGING_ORIGIN GPoint(1 ,2)
#define MOCKED_PAGING_PAGE GRect(3, 4, 5, 6)
bool s_text_layer_calc_text_flow_paging_values_result;
bool text_layer_calc_text_flow_paging_values(const TextLayer *text_layer,
                                             GPoint *content_origin_on_screen,
                                             GRect *page_rect_on_screen) {
  *content_origin_on_screen = MOCKED_PAGING_ORIGIN;
  *page_rect_on_screen = MOCKED_PAGING_PAGE;
  return s_text_layer_calc_text_flow_paging_values_result;
}

// Tests
//////////////////////

#define cl_assert_mocks_called(expected) \
  do { \
  cl_assert_equal_p((expected).disable_text_flow.layout, s_actual.disable_text_flow.layout); \
  cl_assert_equal_p((expected).enable_text_flow.layout, s_actual.enable_text_flow.layout); \
  cl_assert_equal_i((expected).enable_text_flow.inset, s_actual.enable_text_flow.inset); \
  cl_assert_equal_p((expected).disable_paging.layout, s_actual.disable_paging.layout); \
  cl_assert_equal_p((expected).enable_paging.layout, s_actual.enable_paging.layout); \
  cl_assert_equal_i((expected).enable_paging.origin.x, s_actual.enable_paging.origin.x); \
  cl_assert_equal_i((expected).enable_paging.origin.y, s_actual.enable_paging.origin.y); \
  cl_assert_equal_i((expected).enable_paging.paging.origin.x, s_actual.enable_paging.paging.origin.x); \
  cl_assert_equal_i((expected).enable_paging.paging.origin.y, s_actual.enable_paging.paging.origin.y); \
  cl_assert_equal_i((expected).enable_paging.paging.size.w, s_actual.enable_paging.paging.size.w); \
  cl_assert_equal_i((expected).enable_paging.paging.size.h, s_actual.enable_paging.paging.size.h); \
  } while(0)

Window window;
TextLayer text_layer;

void test_text_layer__initialize(void) {
  s_actual = (MockValues){};
  window = (Window){};
  text_layer_init(&text_layer, &GRect(10, 20, 30, 40));
  s_text_layer_calc_text_flow_paging_values_result = true;
}

void test_text_layer__cleanup(void) {
}

void test_text_layer__enable_text_flow_does_nothing_outside_view_hierarchy(void) {
  text_layer_enable_screen_text_flow_and_paging(&text_layer, 8);
  // nothing called
  cl_assert_mocks_called((MockValues){});
  cl_assert(text_layer.layout_cache == NULL);
}

void test_text_layer__enable_text_flow(void) {
  text_layer.layer.window = &window;
  text_layer_enable_screen_text_flow_and_paging(&text_layer, 8);

  cl_assert_mocks_called(((MockValues){
    .enable_text_flow.layout = text_layer.layout_cache,
    .enable_text_flow.inset = 8,
    .enable_paging.layout = text_layer.layout_cache,
    .enable_paging.origin = MOCKED_PAGING_ORIGIN,
    .enable_paging.paging = MOCKED_PAGING_PAGE,
  }));
  cl_assert(text_layer.layout_cache == MOCKED_CREATED_LAYOUT);
}

void test_text_layer__enable_text_flow_requires_successful_calc_for_paging(void) {
  text_layer.layer.window = &window;
  s_text_layer_calc_text_flow_paging_values_result = false;
  text_layer_enable_screen_text_flow_and_paging(&text_layer, 8);
  cl_assert_mocks_called(((MockValues){
    .enable_text_flow.layout = text_layer.layout_cache,
    .enable_text_flow.inset = 8,
  }));
  cl_assert(text_layer.layout_cache == MOCKED_CREATED_LAYOUT);
}

void test_text_layer__disable_text_flow(void) {
  text_layer.layout_cache = MOCKED_CREATED_LAYOUT;
  text_layer_restore_default_text_flow_and_paging(&text_layer);
  cl_assert_mocks_called(((MockValues){
    .disable_paging.layout = text_layer.layout_cache,
    .disable_text_flow.layout = text_layer.layout_cache,
  }));
}
