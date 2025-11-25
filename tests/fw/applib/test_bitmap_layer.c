/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/graphics.h"
#include "applib/ui/bitmap_layer.h"

// Stubs
/////////////////////
#include "stubs_app_state.h"
#include "stubs_graphics.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"

// Fakes
/////////////////////

static GRect s_graphics_draw_bitmap_in_rect__rect = GRectZero;

void graphics_draw_bitmap_in_rect(GContext* ctx, const GBitmap *src_bitmap, const GRect *rect) {
  s_graphics_draw_bitmap_in_rect__rect = *rect;
}

bool process_manager_compiled_with_legacy2_sdk(void) {
  return cl_mock_type(bool);
}

// Test boilerplate
/////////////////////

void test_bitmap_layer__initialize(void) {
}

void test_bitmap_layer__cleanup(void) {
}

// Tests
//////////////////////

// Test inspired by PBL-19136. Check that bitmaps get drawn in the right rect
// on recent SDKs but that a previous bug is kept for 2.x SDK
void test_bitmap_layer__nonzero_bounds(void) {
  GContext ctx = {
    .draw_state = (GDrawState) {
      .clip_box = GRect(0, 0, 144, 168),
      .drawing_box = GRect(0, 0, 144, 168),
    },
  };

  static const GRect BITMAP_LAYER_FRAME = GRect(0, 0, 640, 64);
  static const GRect BITMAP_LAYER_BOUNDS = GRect(-32, 0, 640, 64);
  static const GRect BITMAP_BOUNDS = GRect(0, 0, 640, 64);
  GBitmap bitmap = {
    .bounds = BITMAP_BOUNDS,
  };

  BitmapLayer layer;
  bitmap_layer_init(&layer, &BITMAP_LAYER_FRAME);
  bitmap_layer_set_bitmap(&layer, &bitmap);

  // set bounds with non-zero origin
  layer_set_bounds((Layer *)&layer, &BITMAP_LAYER_BOUNDS);

  // !legacy2
  cl_will_return(process_manager_compiled_with_legacy2_sdk, false);
  layer_render_tree((Layer *)&layer, &ctx);
  GRect expected_rect = BITMAP_BOUNDS;
  cl_assert(grect_equal(&s_graphics_draw_bitmap_in_rect__rect, &expected_rect));
  // legacy2
  cl_will_return(process_manager_compiled_with_legacy2_sdk, true);
  layer_render_tree((Layer *)&layer, &ctx);
  expected_rect = BITMAP_LAYER_BOUNDS;
  cl_assert(grect_equal(&s_graphics_draw_bitmap_in_rect__rect, &expected_rect));
}
