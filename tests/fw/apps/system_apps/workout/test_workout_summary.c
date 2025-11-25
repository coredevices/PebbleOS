/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/workout/workout_summary.h"
#include "apps/system_apps/workout/workout_utils.h"

#include "test_workout_app_includes.h"

// Fakes
/////////////////////

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  return 0;
}

bool workout_service_is_workout_type_supported(ActivitySessionType type) {
  return true;
}

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_workout_summary__initialize(void) {
  // Setup graphics context
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;

  // Setup resources
  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();

  // Setup content indicator
  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}

void test_workout_summary__cleanup(void) {
}

// Helpers
//////////////////////

static void prv_start_workout_cb(ActivitySessionType type) { }
static void prv_select_workout_cb(ActivitySessionType type) { }

static void prv_create_window_and_render(ActivitySessionType activity_type) {
  Window *window = (Window *)workout_summary_window_create(activity_type,
                                                           prv_start_workout_cb,
                                                           prv_select_workout_cb);
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
}

// Tests
//////////////////////

void test_workout_summary__render_open_workout(void) {
  prv_create_window_and_render(ActivitySessionType_Open);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_summary__render_walk(void) {
  prv_create_window_and_render(ActivitySessionType_Walk);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_summary__render_run(void) {
  prv_create_window_and_render(ActivitySessionType_Run);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
