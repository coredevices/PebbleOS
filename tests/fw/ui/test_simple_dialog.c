/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/content_indicator.h"
#include "applib/ui/content_indicator_private.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window_private.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"
#include "shell/system_theme.h"
#include "system/passert.h"
#include "util/graphics.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/size.h"

#include "clar.h"

#include <stdio.h>

static GContext s_ctx;

// Fakes
/////////////////////

#include "fake_content_indicator.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Stubs
/////////////////////

#include "stubs_analytics.h"
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
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscall_internal.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_vibes.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

AnimationProgress animation_timing_scaled(AnimationProgress time_normalized,
                                          AnimationProgress interval_start,
                                          AnimationProgress interval_end) {
  return interval_end;
}

KinoReel *kino_reel_scale_segmented_create(KinoReel *from_reel, bool take_ownership,
                                           GRect screen_frame) {
  return NULL;
}

void kino_reel_scale_segmented_set_deflate_effect(KinoReel *reel, int16_t expand) {}

bool kino_reel_scale_segmented_set_delay_by_distance(KinoReel *reel, GPoint target) {
  return false;
}


// Helper Functions
/////////////////////

#include "../graphics/test_graphics.h"
#include "../graphics/util.h"

// Setup and Teardown
////////////////////////////////////

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

void test_simple_dialog__initialize(void) {
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
}

void test_simple_dialog__cleanup(void) {
  free(fb);

  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = NULL;
}

// Helpers
//////////////////////

void prv_push_and_render_simple_dialog(Dialog *dialog) {
  gbitmap_destroy(s_dest_bitmap);

  const int16_t bitmap_width = DISP_COLS;
  const int16_t bitmap_height = DISP_ROWS;
  const GSize bitmap_size = GSize(bitmap_width, bitmap_height);
  s_dest_bitmap = gbitmap_create_blank(bitmap_size, CANVAS_GBITMAP_FORMAT);

  s_ctx.dest_bitmap = *s_dest_bitmap;
  s_ctx.draw_state.clip_box.size = bitmap_size;
  s_ctx.draw_state.drawing_box.size = bitmap_size;

  // Fill the bitmap with pink (on color) or white (on b&w) so it's easier to see errors
  memset(s_dest_bitmap->addr, PBL_IF_COLOR_ELSE(GColorShockingPinkARGB8, GColorWhiteARGB8),
         s_dest_bitmap->row_size_bytes * s_dest_bitmap->bounds.size.h);

  window_set_on_screen(&dialog->window, true, true);
  window_render(&dialog->window, &s_ctx);
}

// Tests
//////////////////////

void test_simple_dialog__watchface_crashed(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Watchface crashed");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "Watchface is not responding");
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_WARNING_LARGE);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_INFINITE /* no timeout */);

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__battery_charged(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Battery Status");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "Fully Charged");
  dialog_set_background_color(dialog, GColorKellyGreen);
  dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_FULL_LARGE);

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__battery_warning(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Battery Status");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  uint32_t percent = 20;
  dialog_set_background_color(dialog, GColorRed);
  char buffer[25];
  snprintf(buffer, sizeof(buffer), "%d%% Battery", percent);
  dialog_set_text(dialog, buffer);
  dialog_set_icon(dialog, RESOURCE_ID_BATTERY_ICON_LOW_LARGE);

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__ping(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Ping");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_background_color(dialog, GColorCobaltBlue);
  dialog_set_text_color(dialog, GColorWhite);
  dialog_set_text(dialog, "Ping");

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__alarm_snooze(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Alarm Snooze");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  const char *snooze_text = "Snooze delay set to %d minutes";
  char snooze_buf[64];
  snprintf(snooze_buf, sizeof(snooze_buf), snooze_text, 10);
  dialog_set_text(dialog, snooze_buf);
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_CONFIRMATION_LARGE);
  dialog_set_background_color(dialog, GColorJaegerGreen);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_DEFAULT);

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__alarm_deleted(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Alarm Deleted");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "Alarm Deleted");
  dialog_set_icon(dialog, RESOURCE_ID_RESULT_SHREDDED_LARGE);
  dialog_set_background_color(dialog, GColorJaegerGreen);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_DEFAULT);

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__calendar_unmute(void) {
  SimpleDialog *simple_dialog = simple_dialog_create("Calendar Unmute");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  dialog_set_text(dialog, "Calendar Unmuted");
  dialog_set_icon(dialog, RESOURCE_ID_RESULT_MUTE_LARGE);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_DEFAULT);

  prv_push_and_render_simple_dialog(dialog);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_simple_dialog__does_text_fit(void) {
  const bool use_status_bar = true;
  const GSize icon_size = timeline_resources_get_gsize(TimelineResourceSizeLarge);

  char *msg = "1 line error";
  bool text_fits = simple_dialog_does_text_fit(msg, DISP_FRAME.size, icon_size, use_status_bar);
  cl_assert(text_fits);

  msg = "This error fits on all of our displays";
  text_fits = simple_dialog_does_text_fit(msg, DISP_FRAME.size, icon_size, use_status_bar);
  cl_assert(text_fits);

  msg = "This error is too long for rect displays";
  text_fits = simple_dialog_does_text_fit(msg, DISP_FRAME.size, icon_size, use_status_bar);
  PBL_IF_RECT_ELSE(cl_assert(!text_fits), cl_assert(text_fits));

  msg = "This error is too long to fit on any display shape :(";
  text_fits = simple_dialog_does_text_fit(msg, DISP_FRAME.size, icon_size, use_status_bar);
  cl_assert(!text_fits);
}
