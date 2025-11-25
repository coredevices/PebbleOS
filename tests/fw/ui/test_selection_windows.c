/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/time_range_selection_window.h"
#include "applib/ui/time_selection_window.h"
#include "applib/ui/app_window_stack.h"
#include "apps/system_apps/settings/settings_notifications_private.h"
#include "resource/resource.h"
#include "shell/system_theme.h"
#include "system/passert.h"
#include "util/graphics.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/size.h"

#include "clar.h"

#include <stdio.h>

// Helper Functions
/////////////////////

#include "../graphics/test_graphics.h"
#include "../graphics/util.h"

// Fakes
/////////////////////

#include "fake_graphics_context.h"
#include "fake_spi_flash.h"
#include "../../fixtures/load_test_resources.h"

static GContext s_ctx;

void clock_get_time_tm(struct tm* time_tm) {
  rtc_get_time_tm(time_tm);
}

static FrameBuffer *fb = NULL;
static GBitmap *s_dest_bitmap;

// To easily render multiple windows in a single canvas, we'll use an 8-bit bitmap for color
// displays (including round), but we can use the native format for black and white displays (1-bit)
#define CANVAS_GBITMAP_FORMAT PBL_IF_COLOR_ELSE(GBitmapFormat8Bit, GBITMAP_NATIVE_FORMAT)

// Overrides same function in graphics.c; we need to do this so we can pass in the GBitmapFormat
// we need to use for the unit test output canvas instead of relying on GBITMAP_NATIVE_FORMAT, which
// wouldn't work for Spalding since it uses GBitmapFormat8BitCircular
GBitmap* graphics_capture_frame_buffer(GContext *ctx) {
  PBL_ASSERTN(ctx);
  return graphics_capture_frame_buffer_format(ctx, CANVAS_GBITMAP_FORMAT);
}

// Overrides same function in graphics.c; we need to do this so we can release the framebuffer we're
// using even though its format doesn't match GBITMAP_NATIVE_FORMAT (see comment for mocked
// graphics_capture_frame_buffer() above)
bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {
  PBL_ASSERTN(ctx);
  ctx->lock = false;
  framebuffer_dirty_all(ctx->parent_framebuffer);
  return true;
}

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_bootbits.h"
#include "stubs_buffer.h"
#include "stubs_click.h"
#include "stubs_heap.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_menu_cell_layer.h"
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
#include "stubs_status_bar_layer.h"
#include "stubs_syscall_internal.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_text_layer_flow.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Setup and Teardown
////////////////////////////////////

void test_selection_windows__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&s_ctx, fb);
  framebuffer_clear(fb);

  load_system_resources_fixture();
}

void test_selection_windows__cleanup(void) {
  free(fb);

  if (s_dest_bitmap) {
    gbitmap_destroy(s_dest_bitmap);
  }
  s_dest_bitmap = NULL;
}

// Helpers
//////////////////////

#define GRID_CELL_PADDING 5

typedef void (*RenderCallback)(GContext *ctx, SettingsContentSize content_size);

static void prv_prepare_canvas_and_render_for_each_size(RenderCallback callback) {
  GContext *ctx = &s_ctx;

  const unsigned int num_columns = SettingsContentSizeCount;

  const int16_t bitmap_width = (DISP_COLS * num_columns) + (GRID_CELL_PADDING * (num_columns + 1));
  const int16_t bitmap_height = DISP_ROWS;
  const GSize bitmap_size = GSize(bitmap_width, bitmap_height);
  s_dest_bitmap = gbitmap_create_blank(bitmap_size, CANVAS_GBITMAP_FORMAT);

  ctx->dest_bitmap = *s_dest_bitmap;
  ctx->draw_state.clip_box.size = bitmap_size;
  ctx->draw_state.drawing_box.size = bitmap_size;

  // Fill the bitmap with pink (on color) or white (on b&w) so it's easier to see errors
  memset(s_dest_bitmap->addr, PBL_IF_COLOR_ELSE(GColorShockingPinkARGB8, GColorWhiteARGB8),
         s_dest_bitmap->row_size_bytes * s_dest_bitmap->bounds.size.h);

  for (SettingsContentSize content_size = 0; content_size < SettingsContentSizeCount;
       content_size++) {
    system_theme_set_content_size(settings_content_size_to_preferred_size(content_size));
    callback(ctx, content_size);
  }
}

void time_selection_window_set_to_current_time(TimeSelectionWindowData *date_time_window);

void time_selection_window_configure(TimeSelectionWindowData *time_selection_window,
                                     const TimeSelectionWindowConfig *config);

void time_selection_window_init(TimeSelectionWindowData *time_selection_window,
                                const TimeSelectionWindowConfig *config);

#define SELECTION_COLOR PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack)

static const TimeSelectionWindowConfig s_time_config = {
  .label = "Time Config",
  .color = SELECTION_COLOR,
  .range = {
    .update = true,
    .text = "Range text yo!",
    .duration_m = 30,
    .enabled = true,
  },
};

static void prv_render_time_selection_window(GContext *ctx, SettingsContentSize content_size) {
  const int16_t x_offset = GRID_CELL_PADDING + (content_size * (GRID_CELL_PADDING + DISP_COLS));
  ctx->draw_state.drawing_box.origin = GPoint(x_offset, 0);

  TimeSelectionWindowData selection_window;
  time_selection_window_init(&selection_window, &s_time_config);

  selection_window.time_data.hour = 16;
  selection_window.time_data.minute = 20;
  time_selection_window_configure(&selection_window, &s_time_config);

  // Set the window on screen so its window handlers will be called
  window_set_on_screen(&selection_window.window, true, true);

  window_render(&selection_window.window, ctx);
}

static void prv_render_time_range_selection_window(GContext *ctx,
                                                   SettingsContentSize content_size) {
  const int16_t x_offset = GRID_CELL_PADDING + (content_size * (GRID_CELL_PADDING + DISP_COLS));
  ctx->draw_state.drawing_box.origin = GPoint(x_offset, 0);

  TimeRangeSelectionWindowData selection_window;
  time_range_selection_window_init(&selection_window, SELECTION_COLOR, NULL, NULL);

  selection_window.from.hour = 16;
  selection_window.from.minute = 20;
  selection_window.to.hour = 18;
  selection_window.to.minute = 9;

  // Set the window on screen so its window handlers will be called
  window_set_on_screen(&selection_window.window, true, true);

  window_render(&selection_window.window, ctx);
}

// Tests
//////////////////////

void test_selection_windows__time_selection_window(void) {
  prv_prepare_canvas_and_render_for_each_size(prv_render_time_selection_window);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_selection_windows__time_range_selection_window(void) {
  prv_prepare_canvas_and_render_for_each_size(prv_render_time_range_selection_window);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}
