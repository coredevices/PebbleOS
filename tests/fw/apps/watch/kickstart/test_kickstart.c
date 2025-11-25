/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/watch/kickstart/kickstart.h"
#include "popups/timeline/peek.h"

#include "applib/ui/window_private.h"
#include "fw/graphics/util.h"
#include "util/size.h"

#include "clar.h"

extern void prv_window_load_handler(Window *window);
extern void prv_window_unload_handler(Window *window);
extern void prv_set_unobstructed_area_height(int16_t height);
extern void prv_set_data(KickstartData *data, int32_t current_steps,
                         int32_t typical_steps, int32_t daily_steps_avg, int32_t current_bpm);

// Fakes
/////////////////////

#include "fake_pbl_std.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

static bool s_clock_is_24h_style = false;
bool clock_is_24h_style(void) {
  return s_clock_is_24h_style;
}

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_app.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_timer.h"
#include "stubs_app_window_stack.h"
#include "stubs_bootbits.h"
#include "stubs_click.h"
#include "stubs_event_service_client.h"
#include "stubs_health_service.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_info.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_text_layer_flow.h"
#include "stubs_vibes.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) {}

void tick_timer_service_unsubscribe(void) {}

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

static KickstartData s_data;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_kickstart__initialize(void) {
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

  // Reset data
  prv_set_data(&s_data, 0, 0, 0, 0);
  prv_set_unobstructed_area_height(0);
  s_clock_is_24h_style = false;

  // Init window
  window_init(&s_data.window, WINDOW_NAME("Kickstart"));
  window_set_user_data(&s_data.window, &s_data);
  window_set_window_handlers(&s_data.window, &(WindowHandlers){
    .load = prv_window_load_handler,
    .unload = prv_window_unload_handler,
  });
}

void test_kickstart__cleanup(void) {
  window_deinit(&s_data.window);
}

// Tests
//////////////////////

void test_kickstart__render_no_data(void) {
  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_kickstart__render_obstructed_area(void) {
#if !PBL_ROUND
  prv_set_unobstructed_area_height(TIMELINE_PEEK_HEIGHT);
  prv_set_data(&s_data, 5543, 6500, 8000, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_kickstart__render_steps_above_typical(void) {
  prv_set_data(&s_data, 3528, 2500, 8000, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_kickstart__render_steps_below_typical(void) {
  prv_set_data(&s_data, 5543, 6500, 8000, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_kickstart__render_steps_above_daily_avg(void) {
  prv_set_data(&s_data, 10323, 7500, 8000, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_kickstart__render_hr_bpm(void) {
#if (PLATFORM_SILK || PLATFORM_ROBERT)
  prv_set_data(&s_data, 10323, 7500, 13000, 82);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_kickstart__render_hr_bpm_obstructed(void) {
#if (PLATFORM_SILK || PLATFORM_ROBERT)
  prv_set_data(&s_data, 10323, 7500, 13000, 82);
  prv_set_unobstructed_area_height(TIMELINE_PEEK_HEIGHT);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_kickstart__render_steps_above_daily_avg_24h(void) {
  s_clock_is_24h_style = true;
  prv_set_data(&s_data, 10323, 7500, 8000, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_kickstart__render_hr_bpm_24h(void) {
#if (PLATFORM_SILK || PLATFORM_ROBERT)
  s_clock_is_24h_style = true;
  prv_set_data(&s_data, 10323, 7500, 13000, 82);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_kickstart__render_hr_bpm_obstructed_24h(void) {
#if (PLATFORM_SILK || PLATFORM_ROBERT)
  s_clock_is_24h_style = true;
  prv_set_data(&s_data, 10323, 7500, 13000, 82);
  prv_set_unobstructed_area_height(TIMELINE_PEEK_HEIGHT);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
#endif
}

void test_kickstart__render_PBL_43681(void) {
  prv_set_data(&s_data, 0, 1098, 8, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_kickstart__render_PBL_43717(void) {
  prv_set_data(&s_data, 7, 0, 7, 0);

  window_set_on_screen(&s_data.window, true, true);
  window_render(&s_data.window, &s_ctx);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
