/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "fixtures/load_test_resources.h"

#include "applib/fonts/fonts_private.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/text.h"
#include "applib/graphics/text_resources.h"
#include "applib/ui/layer.h"
#include "applib/ui/window_private.h"
#include "resource/resource_ids.auto.h"

#include <stdio.h>


// Helper Functions
////////////////////////////////////
#include "test_graphics.h"
#include "${BIT_DEPTH_NAME}/test_framebuffer.h"
#include "util.h"

///////////////////////////////////////////////////////////
// Stubs
#include "stubs_analytics.h"
#include "stubs_app_state.h"
#include "stubs_applib_resource.h"
#include "stubs_bootbits.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_syscall_internal.h"
#include "stubs_syscalls.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"

///////////////////////////////////////////////////////////
// Fakes
#include "fake_gbitmap_get_data_row.h"

static FrameBuffer *fb = NULL;

// Setup
void test_graphics_draw_text_${BIT_DEPTH_NAME}__initialize(void) {
  s_fake_data_row_handling = false;
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
}

// Teardown
void test_graphics_draw_text_${BIT_DEPTH_NAME}__cleanup(void) {
  free(fb);
}

///////////////////////////////////////////////////////////
// Tests

static FontInfo s_font_info;
static const char *s_text_buffer = "Text Clipping";

static void prv_setup_resources(void) {
  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false /* is_next */);

  resource_init();
}

// Corner Tests
void draw_text_single_line_ellipsis_clip_across_nx_zero_y_offset(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(-44, 0, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_across_ny_descender(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(0, -25, 100, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Ellipsis Tests
void draw_text_single_line_ellipsis_clip_across_ny(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, -18, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_across_y(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, 20, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_across_nx(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(-44, 4, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_across_x(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(34, 4, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_outside_ny(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, -40, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_outside_y(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, 40, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_outside_nx(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(-80, 4, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_ellipsis_clip_outside_x(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(80, 4, 72, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Word Wrap Tests
void draw_text_single_line_wordwrap_clip_across_ny(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, -18, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_across_ny_second_line(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, -46, 72, 50),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_across_y(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, 20, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_across_y_second_line(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, -10, 72, 50),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_across_nx(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(-44, 4, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_across_x(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(34, 4, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_outside_ny(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, -40, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_outside_y(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(4, 40, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_outside_nx(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(-80, 4, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void draw_text_single_line_wordwrap_clip_outside_x(Layer* me, GContext* ctx) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, s_text_buffer, &s_font_info, GRect(80, 4, 72, 32),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void canvas_layer_update_callback(Layer* me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, &GRect(39, 39, 82, 42));
}

void test_graphics_draw_text_${BIT_DEPTH_NAME}__clipping(void) {
  GContext ctx;
  Layer canvas;
  Layer layer;

  prv_setup_resources();

  uint32_t gothic_24_handle = RESOURCE_ID_GOTHIC_24_BOLD;
  cl_assert(text_resources_init_font(0, gothic_24_handle, 0, &s_font_info));

  test_graphics_context_init(&ctx, fb);

  layer_init(&canvas, &GRect(0, 0, 144, 168));
  layer_set_update_proc(&canvas, &canvas_layer_update_callback);

  layer_init(&layer, &GRect(40, 40, 80, 40));
  layer_add_child(&canvas, &layer);

  // Corner cases
  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_across_nx_zero_y_offset);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_across_nx_zero_y_offset.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_across_ny_descender);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_across_ny_descender.${BIT_DEPTH_NAME}.pbi"));

  // Ellipsis tests
  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_across_ny);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_across_ny.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_across_y);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_across_y.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_across_nx);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_across_nx.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_across_x);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_across_x.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_outside_ny);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_outside_ny.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_outside_y);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_outside_y.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_outside_nx);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_outside_nx.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_ellipsis_clip_outside_x);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_ellipsis_clip_outside_x.${BIT_DEPTH_NAME}.pbi"));

  // Word Wrap tests
  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_across_ny);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_across_ny.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_across_ny_second_line);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_across_ny_second_line.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_across_y);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_across_y.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_across_y_second_line);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_across_y_second_line.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_across_nx);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_across_nx.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_across_x);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_across_x.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_outside_ny);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_outside_ny.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_outside_y);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_outside_y.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_outside_nx);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_outside_nx.${BIT_DEPTH_NAME}.pbi"));

  test_graphics_context_reset(&ctx, fb);
  layer_set_update_proc(&layer, &draw_text_single_line_wordwrap_clip_outside_x);
  layer_render_tree(&canvas, &ctx);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_single_line_wordwrap_clip_outside_x.${BIT_DEPTH_NAME}.pbi"));
}

#define RECT_TEXT_0_0 GRect(0, 0, 140, 1000)
#define RECT_TEXT_2_0 GRect(2, 0, 140, 1000)

void test_graphics_draw_text_${BIT_DEPTH_NAME}__clipping_letters(void) {
  GContext ctx;

  prv_setup_resources();

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18_BOLD;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  test_graphics_context_init(&ctx, fb);

  // Test when clipping/drawing and text bounds are all at (0, 0)
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_0_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_jja00.${BIT_DEPTH_NAME}.pbi"));

  // Test when clipping/drawing are at (2, 0) and text bounds is at (0, 0)
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_2_0, RECT_TEXT_2_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_0_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_jja20.${BIT_DEPTH_NAME}.pbi"));

  // Test when clipping/drawing and text bounds are all at (2, 0)
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_2_0, RECT_TEXT_2_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_2_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_jja22.${BIT_DEPTH_NAME}.pbi"));

  // Test when clipping/drawing and text bounds are all at (2, 0)
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_2_0, RECT_TEXT_2_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "ajj", &s_font_info, RECT_TEXT_2_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_ajj22.${BIT_DEPTH_NAME}.pbi"));

  // Test when clipping/drawing and text bounds are all at (2, 0) - no negative offset
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_2_0, RECT_TEXT_2_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "aaa", &s_font_info, RECT_TEXT_2_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_aaa22.${BIT_DEPTH_NAME}.pbi"));
}

#define RECT_NULL GRect(0, 0, 0, 0)
#define RECT_NULL_W GRect(0, 0, 0, 20)
#define RECT_NULL_H GRect(0, 0, 20, 0)
#define RECT_NEG GRect(0, 0, -20, -20)
#define RECT_NEG_W GRect(0, 0, -20, 0)
#define RECT_NEG_H GRect(0, 0, 0, -20)
void test_graphics_draw_text_${BIT_DEPTH_NAME}__zero(void) {
  GContext ctx;

  prv_setup_resources();

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18_BOLD;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  test_graphics_context_init(&ctx, fb);

  // Test zero text bounds size - ensure nothing is rendered
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NULL,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NULL_W,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_w", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NULL_H,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_h", fb, GColorWhite));

  // Test negative text bounds size - ensure nothing is rendered
  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NEG,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_neg", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NEG_W,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_neg_w", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NEG_H,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_neg_h", fb, GColorWhite));

  // Test null context boxes - ensure nothing is rendered
  setup_test_aa_sw(&ctx, fb, RECT_NULL, RECT_NULL, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_0_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_null", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_NULL_W, RECT_NULL_W, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_0_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_null", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_NULL_H, RECT_NULL_H, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_0_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_null", fb, GColorWhite));

  // Test negative context boxes - ensure nothing is rendered
  setup_test_aa_sw(&ctx, fb, RECT_NEG, RECT_NEG, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_TEXT_0_0,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_null", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_NEG, RECT_NEG, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NEG,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_null", fb, GColorWhite));

  setup_test_aa_sw(&ctx, fb, RECT_NEG, RECT_NEG, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);
  graphics_draw_text(&ctx, "jja", &s_font_info, RECT_NULL,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(framebuffer_is_empty("draw_text_null_null", fb, GColorWhite));
}

void test_graphics_draw_text_8bit__color(void) {
  GContext ctx;

  prv_setup_resources();

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18_BOLD;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  test_graphics_context_init(&ctx, fb);

  setup_test_aa_sw(&ctx, fb, GRect(0, 0, 144, 168), GRect(0, 0, 144, 168), false, 1);
  graphics_context_set_fill_color(&ctx, GColorRed);
  graphics_fill_rect(&ctx, &GRect(0, 0, 144, 168));
  graphics_context_set_text_color(&ctx, GColorBlue);
  graphics_draw_text(&ctx, "blue100", &s_font_info, GRect(10, 10, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ctx.draw_state.text_color.a = 2;
  graphics_draw_text(&ctx, "blue66", &s_font_info, GRect(10, 40, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ctx.draw_state.text_color.a = 1;
  graphics_draw_text(&ctx, "blue33", &s_font_info, GRect(10, 70, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ctx.draw_state.text_color.a = 0;
  graphics_draw_text(&ctx, "blue0", &s_font_info, GRect(10, 100, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_color_assign.8bit.pbi"));

  setup_test_aa_sw(&ctx, fb, GRect(0, 0, 144, 168), GRect(0, 0, 144, 168), false, 1);
  graphics_context_set_fill_color(&ctx, GColorRed);
  graphics_fill_rect(&ctx, &GRect(0, 0, 144, 168));
  graphics_context_set_compositing_mode(&ctx, GCompOpSet);
  graphics_context_set_text_color(&ctx, GColorBlue);
  graphics_draw_text(&ctx, "blue100", &s_font_info, GRect(10, 10, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ctx.draw_state.text_color.a = 2;
  graphics_draw_text(&ctx, "blue66", &s_font_info, GRect(10, 40, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ctx.draw_state.text_color.a = 1;
  graphics_draw_text(&ctx, "blue33", &s_font_info, GRect(10, 70, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  ctx.draw_state.text_color.a = 0;
  graphics_draw_text(&ctx, "blue0", &s_font_info, GRect(10, 100, 100, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_color_set.8bit.pbi"));
}

void test_graphics_draw_text_8bit__data_row_offsets(void) {
  GContext ctx;

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false /* is_next */);

  resource_init();

  memset(&s_font_info, 0, sizeof(s_font_info));

  uint32_t gothic_18_handle = RESOURCE_ID_GOTHIC_18_BOLD;
  cl_assert(text_resources_init_font(0, gothic_18_handle, 0, &s_font_info));

  test_graphics_context_init(&ctx, fb);

  // Enable fake data row handling which will override the gbitmap_get_data_row_xxx() functions
  // with their fake counterparts in fake_gbitmap_get_data_row.c
  s_fake_data_row_handling = true;

  // The following test uses fake bitmap data row handling to clip the text rendering of a repeated
  // string of alphabet characters to a diamond mask which is flipped vertically

  setup_test_aa_sw(&ctx, fb, RECT_TEXT_0_0, RECT_TEXT_0_0, false, 1);
  graphics_context_set_text_color(&ctx, GColorBlack);

  graphics_draw_text(&ctx, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR"
                           "STUVWXYZabcdefghijklmnopqrstuvwxyz", &s_font_info, RECT_TEXT_0_0,
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  cl_check(gbitmap_pbi_eq(&ctx.dest_bitmap, "draw_text_data_row_offsets.8bit.pbi"));
}
