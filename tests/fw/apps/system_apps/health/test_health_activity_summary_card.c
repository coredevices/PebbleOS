/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system/health/data.h"
#include "apps/system/health/data_private.h"
#include "apps/system/health/activity_summary_card.h"
#include "apps/system/health/activity_detail_card.h"
#include "apps/system/health/detail_card.h"

#include "test_health_app_includes.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Override fake_clock's WEAK default so the split layout's bin time renders
// in a stable 24h format.
bool clock_is_24h_style(void) {
  return true;
}

void test_health_activity_summary_card__initialize(void) {
  // Pin RTC to 2024-01-06 16:19:35 UTC (Saturday 16:19 -> bin minute 975 = 16:15).
  rtc_set_time(1704557975);

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

void test_health_activity_summary_card__cleanup(void) {
}

// Helpers
//////////////////////

static void prv_create_card_and_render(HealthData *health_data) {
  Window window;
  window_init(&window, WINDOW_NAME("Health"));
  Layer *window_layer = window_get_root_layer(&window);
  Layer *card_layer = health_activity_summary_card_create(health_data);
  layer_set_frame(card_layer, &window_layer->bounds);
  layer_add_child(window_layer, card_layer);
  window_set_background_color(&window, health_activity_summary_card_get_bg_color(card_layer));
  window_set_on_screen(&window, true, true);
  window_render(&window, &s_ctx);
}

// Tests
//////////////////////

void test_health_activity_summary_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthData) {});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__no_current_steps(void) {
  HealthData health_data = {
    .step_data = 0,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 750,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical1(void) {
  HealthData health_data = {
    .step_data = 170,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 340,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical2(void) {
  HealthData health_data = {
    .step_data = 320,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 340,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical3(void) {
  HealthData health_data = {
    .step_data = 460,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 555,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical4(void) {
  HealthData health_data = {
    .step_data = 699,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 840,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical5(void) {
  HealthData health_data = {
    .step_data = 837,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 914,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_equals_typical(void) {
  HealthData health_data = {
    .step_data = 837,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 837,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical1(void) {
  HealthData health_data = {
    .step_data = 340,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 170,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical2(void) {
  HealthData health_data = {
    .step_data = 400,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 379,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical3(void) {
  HealthData health_data = {
    .step_data = 780,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 480,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical4(void) {
  HealthData health_data = {
    .step_data = 866,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 700,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical5(void) {
  HealthData health_data = {
    .step_data = 970,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 900,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_expected(void) {
  HealthData health_data = {
    .step_data = {2000},
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 800,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

