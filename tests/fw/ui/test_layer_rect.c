/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/layer.h"
#include "applib/ui/layer_private.h"

#include "clar.h"
#include "pebble_asserts.h"

// Stubs
////////////////////////////////////
#include "stubs_app_state.h"
#include "stubs_bitblt.h"
#include "stubs_gbitmap.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_unobstructed_area.h"

GDrawState graphics_context_get_drawing_state(GContext *ctx) {
  return (GDrawState) { 0 };
}

bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {
  return false;
}

void graphics_context_set_drawing_state(GContext *ctx, GDrawState draw_state) {
}

void window_schedule_render(struct Window *window) {
}

static bool s_process_manager_compiled_with_legacy2_sdk;

bool process_manager_compiled_with_legacy2_sdk(void) {
  return s_process_manager_compiled_with_legacy2_sdk;
}

// Setup
////////////////////////////////////

void test_layer_rect__initialize(void) {
  s_process_manager_compiled_with_legacy2_sdk = false;
}

void test_layer_rect__cleanup(void) {
}

// Tests
////////////////////////////////////

void test_layer_rect__2_x_extend_shrink(void) {
  Layer l;
  s_process_manager_compiled_with_legacy2_sdk = true;

  layer_init(&l, &GRect(10, 20, 30, 40));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 30, 40));

  // expands
  layer_set_frame(&l, &GRect(10, 20, 300, 400));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 300, 400));

  // only expands .h, keeps .w
  layer_set_frame(&l, &GRect(10, 20, 200, 500));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 300, 500));
}

void test_layer_rect__3_x_sync_if_applicable(void) {
  Layer l;
  s_process_manager_compiled_with_legacy2_sdk = false;

  layer_init(&l, &GRect(10, 20, 30, 40));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 30, 40));

  // expands
  layer_set_frame(&l, &GRect(10, 20, 300, 400));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 300, 400));

  // keeps size in sync
  layer_set_frame(&l, &GRect(10, 20, 200, 500));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 200, 500));

  // act as 2.x once bounds.origin is different from (0, 0)
  layer_set_bounds(&l, &GRect(1, 1, 200, 500));
  layer_set_frame(&l, &GRect(10, 20, 100, 600));
  cl_assert_equal_grect(l.bounds, GRect(1, 1, 200, 599));

  // act as 2.x once bounds.size isn't same as frame.size
  layer_set_bounds(&l, &GRect(0, 0, 150, 600));
  layer_set_frame(&l, &GRect(10, 20, 100, 700));
  cl_assert_equal_grect(l.bounds, GRect(0, 0, 150, 700));
}
