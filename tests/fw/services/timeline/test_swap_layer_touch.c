/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Unit tests for swap_layer's touch drag-to-scroll (1:1 within the current
// notification, clamped to its bounds). Drives the real swap_layer touch
// handler by capturing it through the click-config-provider subscription path,
// and asserts the current layout's frame offset. The surrounding rendering /
// animation / window machinery is stubbed.

#include "clar.h"

#include "pbl/services/timeline/swap_layer.h"
#include "pbl/services/timeline/layout_layer.h"
#include "applib/graphics/graphics.h"
#include "applib/touch_service.h"
#include "applib/ui/layer.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// stubs_animation.c and stubs_click.c are compiled in via the wscript.
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

// --- real behavior needed by the touch path ---------------------------------

// Minimal real frame setter so the test can assert the resulting offset.
void layer_set_frame(Layer *layer, const GRect *frame) {
  layer->frame = *frame;
}

// --- link-only no-op stubs (not exercised by the touch path) ----------------

void layer_deinit(Layer *layer) {}
void layer_remove_from_parent(Layer *child) {}
void layer_insert_below_sibling(Layer *a, Layer *b) {}
void layer_set_hidden(Layer *layer, bool hidden) {}

void *applib_malloc(size_t bytes) { return NULL; }
void applib_free(void *ptr) {}

// Rendering / layout / interpolation: only reached on draw, which the tests
// never trigger, so link-only no-ops are sufficient.
GContext *graphics_context_get_current_context(void) { return NULL; }
void graphics_context_set_fill_color(GContext *ctx, GColor color) {}
void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode) {}
void graphics_fill_rect(GContext *ctx, const GRect *rect) {}
void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect) {}
void grect_align(GRect *rect, const GRect *inside_rect, const GAlign alignment, const bool clip) {}
bool gbitmap_init_with_resource(GBitmap *bitmap, uint32_t resource_id) { return true; }
void gbitmap_deinit(GBitmap *bitmap) {}
const LayoutColors *layout_get_colors(const LayoutLayer *layout) { return NULL; }
GSize layout_get_size(GContext *ctx, LayoutLayer *layout) { return GSize(0, 0); }
int64_t interpolate_moook(int32_t normalized, int64_t from, int64_t to) { return to; }

// --- captured window click-config provider ----------------------------------

static ClickConfigProvider s_click_config_provider;
static void *s_ccp_context;
void window_set_click_config_provider_with_context(Window *window,
                                                   ClickConfigProvider provider, void *context) {
  s_click_config_provider = provider;
  s_ccp_context = context;
}
void window_raw_click_subscribe(ButtonId b, ClickHandler d, ClickHandler u, void *c) {}
void window_single_repeating_click_subscribe(ButtonId b, uint16_t ms, ClickHandler h) {}
void window_multi_click_subscribe(ButtonId b, uint8_t mn, uint8_t mx, uint16_t t, bool l,
                                  ClickHandler h) {}

// --- captured touch subscription --------------------------------------------

static TouchServiceHandler s_touch_handler;
static void *s_touch_context;
static bool s_touch_enabled;
static bool s_touch_unsubscribed;
void touch_service_subscribe(TouchServiceHandler handler, void *context) {
  s_touch_handler = handler;
  s_touch_context = context;
}
void touch_service_unsubscribe(void) { s_touch_unsubscribed = true; }
bool touch_service_is_enabled(void) { return s_touch_enabled; }

// --- test scaffolding -------------------------------------------------------

static SwapLayer s_swap_layer;
static LayoutLayer s_current;

// Build a swap layer whose viewport is `viewport_h` tall showing a notification
// `content_h` tall, then run the click-config provider so the touch handler is
// subscribed and captured. max scroll offset is content_h - viewport_h.
static void prv_setup(int16_t content_h, int16_t viewport_h) {
  memset(&s_swap_layer, 0, sizeof(s_swap_layer));
  memset(&s_current, 0, sizeof(s_current));
  s_swap_layer.layer.frame = GRect(0, 0, 100, viewport_h);
  s_current.layer.frame = GRect(0, 0, 100, content_h);
  s_swap_layer.current = &s_current;
  s_swap_layer.next = NULL;
  s_swap_layer.animation = NULL;

  s_touch_enabled = true;
  swap_layer_set_click_config_onto_window(&s_swap_layer, NULL);
  cl_assert(s_click_config_provider != NULL);
  s_click_config_provider(s_ccp_context);
}

static void prv_touch(TouchEventType type, int16_t x, int16_t y) {
  TouchEvent e = { .type = type, .x = x, .y = y };
  cl_assert(s_touch_handler != NULL);
  s_touch_handler(&e, s_touch_context);
}

// Current scroll offset == -(current frame origin.y).
static int16_t prv_offset(void) {
  return -s_current.layer.frame.origin.y;
}

// Preposition the current notification at scroll offset `offset`.
static void prv_set_offset(int16_t offset) {
  s_current.layer.frame.origin.y = -offset;
}

void test_swap_layer_touch__initialize(void) {
  s_click_config_provider = NULL;
  s_ccp_context = NULL;
  s_touch_handler = NULL;
  s_touch_context = NULL;
  s_touch_enabled = false;
  s_touch_unsubscribed = false;
}

void test_swap_layer_touch__cleanup(void) {}

// --- direction --------------------------------------------------------------

// Dragging the finger up reveals content further down: offset increases.
void test_swap_layer_touch__drag_up_scrolls_content_down(void) {
  prv_setup(400 /* content */, 168 /* viewport */);  // max offset 232

  prv_touch(TouchEvent_Touchdown, 50, 150);
  prv_touch(TouchEvent_PositionUpdate, 50, 100);  // finger up 50px
  cl_assert_equal_i(prv_offset(), 50);

  prv_touch(TouchEvent_Liftoff, 50, 100);
  cl_assert_equal_i(prv_offset(), 50);  // committed
}

// Dragging the finger down reveals earlier content: offset decreases.
void test_swap_layer_touch__drag_down_scrolls_content_up(void) {
  prv_setup(400, 168);
  prv_set_offset(100);

  prv_touch(TouchEvent_Touchdown, 50, 100);
  prv_touch(TouchEvent_PositionUpdate, 50, 130);  // finger down 30px
  cl_assert_equal_i(prv_offset(), 70);
}

// --- threshold --------------------------------------------------------------

void test_swap_layer_touch__sub_threshold_move_does_not_scroll(void) {
  prv_setup(400, 168);

  prv_touch(TouchEvent_Touchdown, 50, 150);
  prv_touch(TouchEvent_PositionUpdate, 50, 145);  // only 5px (< 10px threshold)
  cl_assert_equal_i(prv_offset(), 0);
}

// --- clamping ---------------------------------------------------------------

void test_swap_layer_touch__drag_clamps_at_top(void) {
  prv_setup(400, 168);  // already at offset 0 (top)

  prv_touch(TouchEvent_Touchdown, 50, 100);
  prv_touch(TouchEvent_PositionUpdate, 50, 160);  // finger down 60px -> would be -60
  cl_assert_equal_i(prv_offset(), 0);  // clamped at the top
}

void test_swap_layer_touch__drag_clamps_at_bottom(void) {
  prv_setup(400, 168);  // max offset 232
  prv_set_offset(200);

  prv_touch(TouchEvent_Touchdown, 50, 200);
  prv_touch(TouchEvent_PositionUpdate, 50, 100);  // finger up 100px -> would be 300
  cl_assert_equal_i(prv_offset(), 232);  // clamped at max
}

// --- gesture lifecycle ------------------------------------------------------

void test_swap_layer_touch__position_update_after_liftoff_ignored(void) {
  prv_setup(400, 168);

  prv_touch(TouchEvent_Touchdown, 50, 150);
  prv_touch(TouchEvent_PositionUpdate, 50, 100);  // offset 50
  prv_touch(TouchEvent_Liftoff, 50, 100);
  cl_assert_equal_i(prv_offset(), 50);

  // A stray position update once the gesture has ended must not move anything.
  prv_touch(TouchEvent_PositionUpdate, 50, 50);
  cl_assert_equal_i(prv_offset(), 50);
}

// --- subscription gating ----------------------------------------------------

void test_swap_layer_touch__touch_disabled_does_not_subscribe(void) {
  memset(&s_swap_layer, 0, sizeof(s_swap_layer));
  memset(&s_current, 0, sizeof(s_current));
  s_swap_layer.current = &s_current;

  s_touch_enabled = false;  // no touchscreen / touch off
  swap_layer_set_click_config_onto_window(&s_swap_layer, NULL);
  cl_assert(s_click_config_provider != NULL);
  s_click_config_provider(s_ccp_context);

  cl_assert(s_touch_handler == NULL);  // did not subscribe
}

// --- pull-to-swap (edge over-pull -> adjacent notification) -----------------

// Over-pulling past the bottom edge swaps to the next notification.
void test_swap_layer_touch__overpull_at_bottom_swaps_to_next(void) {
  prv_setup(400, 168);  // max scroll offset 232
  static LayoutLayer next_layout;
  memset(&next_layout, 0, sizeof(next_layout));
  next_layout.layer.frame = GRect(0, 0, 100, 300);
  s_swap_layer.next = &next_layout;
  prv_set_offset(232);  // already at the bottom

  prv_touch(TouchEvent_Touchdown, 50, 200);
  prv_touch(TouchEvent_PositionUpdate, 50, 100);  // finger up 100px past the edge
  prv_touch(TouchEvent_Liftoff, 50, 100);

  // Swap-down shifted current <- next.
  cl_assert(s_swap_layer.current == &next_layout);
}

// A drag that only reaches the edge (no over-pull past the margin) does not swap.
void test_swap_layer_touch__reaching_edge_without_overpull_does_not_swap(void) {
  prv_setup(400, 168);
  static LayoutLayer next_layout;
  memset(&next_layout, 0, sizeof(next_layout));
  s_swap_layer.next = &next_layout;
  LayoutLayer *current_before = s_swap_layer.current;
  prv_set_offset(200);  // near the bottom (max 232)

  // Finger up 30px -> requested offset 230, still within [0, 232+overpull].
  prv_touch(TouchEvent_Touchdown, 50, 150);
  prv_touch(TouchEvent_PositionUpdate, 50, 120);
  prv_touch(TouchEvent_Liftoff, 50, 120);

  cl_assert(s_swap_layer.current == current_before);  // no swap
}

// Over-pulling past the top edge attempts a swap to the previous notification
// (which fetches the -1 layout via the client's get_layout_handler).
static int8_t s_last_fetch_rel;
static int s_fetch_count;
static LayoutLayer *prv_get_layout(SwapLayer *swap_layer, int8_t rel, void *context) {
  s_last_fetch_rel = rel;
  s_fetch_count++;
  return NULL;  // no previous available -> graceful, but the attempt is observed
}

void test_swap_layer_touch__overpull_at_top_attempts_previous_swap(void) {
  prv_setup(400, 168);
  s_swap_layer.callbacks.get_layout_handler = prv_get_layout;
  s_fetch_count = 0;
  prv_set_offset(0);  // at the top

  prv_touch(TouchEvent_Touchdown, 50, 100);
  prv_touch(TouchEvent_PositionUpdate, 50, 200);  // finger down 100px past the edge
  prv_touch(TouchEvent_Liftoff, 50, 200);

  // The up-swap path fetched the previous (rel -1) layout.
  cl_assert(s_fetch_count > 0);
  cl_assert_equal_i(s_last_fetch_rel, -1);
}

// --- deinit -----------------------------------------------------------------

void test_swap_layer_touch__deinit_unsubscribes(void) {
  prv_setup(400, 168);
  cl_assert(s_touch_handler != NULL);

  swap_layer_deinit(&s_swap_layer);
  cl_assert(s_touch_unsubscribed);
}
