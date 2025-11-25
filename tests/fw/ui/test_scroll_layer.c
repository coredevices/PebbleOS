/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/scroll_layer.h"

#include "clar.h"

// Stubs
////////////////////////////////////
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_content_indicator.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_unobstructed_area.h"

#define DEFAULT_SCROLL_HEIGHT 32

// Stubs
////////////////////////////////////
static GRect s_graphics_draw_bitmap_in_rect__rect = GRectZero;

void graphics_draw_bitmap_in_rect(GContext* ctx, const GBitmap *src_bitmap, const GRect *rect) {
  s_graphics_draw_bitmap_in_rect__rect = *rect;
}
bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {return false;}
void window_schedule_render(struct Window *window) {}
void window_set_click_config_provider_with_context(
    struct Window *window, ClickConfigProvider click_config_provider, void *context) {}
void window_set_click_context(ButtonId button_id, void *context) {}
void window_single_repeating_click_subscribe(
    ButtonId button_id, uint16_t repeat_interval_ms, ClickHandler handler) {}

// Internal definitions
////////////////////////////////////
extern bool prv_scroll_layer_is_paging_enabled(ScrollLayer *scroll_layer);
extern void prv_scroll_layer_set_content_offset_internal(ScrollLayer *scroll_layer, GPoint offset);
extern uint16_t prv_scroll_layer_get_paging_height(ScrollLayer *scroll_layer);

// Setup
////////////////////////////////////

void test_scroll_layer__initialize(void) {}
void test_scroll_layer__cleanup(void) {}

// Tests
////////////////////////////////////

void test_scroll_layer__enable_paging(void) {
  GRect scroll_bounds = GRect(0,0,180,180);
  ScrollLayer *scroll_layer = scroll_layer_create(scroll_bounds);

  // Verify paging is disabled by default
  cl_assert_equal_b(false, prv_scroll_layer_is_paging_enabled(scroll_layer));
  // Verify shadow_layer is not hidden when paging disabled
  cl_assert_equal_b(false, scroll_layer_get_shadow_hidden(scroll_layer));

  scroll_layer_set_paging(scroll_layer, true);

  // Verify paging is enabled
  cl_assert_equal_b(true, prv_scroll_layer_is_paging_enabled(scroll_layer));
  // Verify shadow_layer is hidden now that paging enabled
  cl_assert_equal_b(true, scroll_layer_get_shadow_hidden(scroll_layer));

  // verify disable paging works
  scroll_layer_set_paging(scroll_layer, false);
  cl_assert_equal_b(false, prv_scroll_layer_is_paging_enabled(scroll_layer));
  // verify shadow layer is hidden on paging disabled
  cl_assert_equal_b(true, scroll_layer_get_shadow_hidden(scroll_layer));
}

void test_scroll_layer__paging_vs_shadow_bits(void) {
  ScrollLayer *scroll_layer = scroll_layer_create(GRect(0,0,180,180));

  // Validate that paging_disabled is same position as shadow clips
  scroll_layer->shadow_sublayer.clips = true;
  cl_assert_equal_b(true, scroll_layer->paging.paging_disabled);
  cl_assert_equal_b(false, scroll_layer->paging.shadow_hidden);

  scroll_layer->shadow_sublayer.clips = false;
  cl_assert_equal_b(false, scroll_layer->paging.paging_disabled);
  cl_assert_equal_b(false, scroll_layer->paging.shadow_hidden);

  // Validate that shadow_hidden is same position as layer hidden in shadow sublayer
  scroll_layer->shadow_sublayer.hidden = true;
  cl_assert_equal_b(false, scroll_layer->paging.paging_disabled);
  cl_assert_equal_b(true, scroll_layer->paging.shadow_hidden);

  scroll_layer->shadow_sublayer.hidden = false;
  cl_assert_equal_b(false, scroll_layer->paging.paging_disabled);
  cl_assert_equal_b(false, scroll_layer->paging.shadow_hidden);

}

void test_scroll_layer__scrolling(void) {
  GRect scroll_bounds = GRect(0,0,180,180);
  ScrollLayer *scroll_layer = scroll_layer_create(scroll_bounds);

  GSize content_size = GSize(180, 2000);
  scroll_layer_set_content_size(scroll_layer, content_size);

  int32_t scroll_height = DEFAULT_SCROLL_HEIGHT;

  int32_t offset = 0;
  
  for (offset = 0; offset < content_size.h - scroll_bounds.size.h; offset += scroll_height) {
    // scroll offset for scroll down is negative, so invert offset.y
    cl_assert_equal_i(offset, -((int32_t)scroll_layer_get_content_offset(scroll_layer).y));
    scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
  }

  // can only scroll to content offset == content_size.h - bounds.size.h
  // so the last scroll from the above loop is expected to have stopped short
  cl_assert(offset > -((int32_t)scroll_layer_get_content_offset(scroll_layer).y));
}

void test_scroll_layer__paging_with_scroll(void) {
  ScrollLayer *scroll_layer = scroll_layer_create(GRect(0,0,180,180));
  int16_t page_height = 0;

  page_height = scroll_layer->layer.frame.size.h;
  scroll_layer_set_paging(scroll_layer, true);
  cl_assert_equal_i(page_height, prv_scroll_layer_get_paging_height(scroll_layer));
  
  // paging should force < page_height offsets to ceil of modulo page height
  scroll_layer_set_content_size(scroll_layer, GSize(180, 2000));
  scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
  // scroll offset for scroll down is negative, so invert offset.y
  cl_assert_equal_i(page_height, -((int32_t)scroll_layer_get_content_offset(scroll_layer).y));
}

void test_scroll_layer__paging_last_pages_content(void) {
  uint16_t page_height = 86;
  ScrollLayer *scroll_layer = scroll_layer_create(GRect(0,0,180,page_height));

  // validate enable paging works for paging height
  scroll_layer_set_paging(scroll_layer, true);
  cl_assert_equal_i(page_height, prv_scroll_layer_get_paging_height(scroll_layer));

  int pages = 2;
  int offset = 0;
  // setup content size to be slightly more than 2 pages
  GSize content_size = GSize(180, page_height * pages + 10);
  scroll_layer_set_content_size(scroll_layer, content_size);

  // paging should force full contents of last page to show
  // so content size rounded up to the next modulo of page_height
  cl_assert_equal_i(offset, -scroll_layer_get_content_offset(scroll_layer).y);

  scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
  offset += page_height;
  cl_assert_equal_i(offset, -scroll_layer_get_content_offset(scroll_layer).y);

  // we expect to scroll to the end of content padded to the last full page
  scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
  offset += page_height;
  cl_assert_equal_i(offset, -scroll_layer_get_content_offset(scroll_layer).y);
  cl_assert_equal_i(page_height * pages, -scroll_layer_get_content_offset(scroll_layer).y);

  // once the last full page of content has been displayed
  // another scroll down shouldn't advance the offset
  scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
  cl_assert_equal_i(page_height * pages, -scroll_layer_get_content_offset(scroll_layer).y);
}

void test_scroll_layer__fullscreen_paging(void) {
  GRect scroll_bounds = GRect(0,0,180,180);
  ScrollLayer *scroll_layer = scroll_layer_create(scroll_bounds);

  int16_t page_height = scroll_bounds.size.h;
  scroll_layer_set_paging(scroll_layer, true);
  cl_assert_equal_i(page_height, prv_scroll_layer_get_paging_height(scroll_layer));

  int pages = 22;
  int offset = 0;
  // setup content size to be slightly more than the pages
  GSize content_size = GSize(scroll_bounds.size.w, page_height * pages + 24);
  scroll_layer_set_content_size(scroll_layer, content_size);

  // paging should force full contents of last page to show
  // so content size rounded up to the next modulo of page_height
  cl_assert_equal_i(offset, -scroll_layer_get_content_offset(scroll_layer).y);

  // we expect to scroll to the end of content padded to the last full page
  for (int i = 0; i < pages; i++) {
    scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
    offset += page_height;
    cl_assert_equal_i(offset, -scroll_layer_get_content_offset(scroll_layer).y);
  }

  // once the last full page of content has been displayed
  // another scroll down shouldn't advance the offset
  scroll_layer_scroll(scroll_layer, ScrollDirectionDown, false);
  cl_assert_equal_i(page_height * pages, -scroll_layer_get_content_offset(scroll_layer).y);
}
