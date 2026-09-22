/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system/health/data.h"
#include "apps/system/health/data_private.h"
#include "apps/system/health/activity_summary_card.h"
#include "apps/system/health/activity_detail_card.h"
#include "apps/system/health/detail_card.h"
#include "apps/system/health/weight_detail_card.h"
#include "apps/system/health/weight_entry_ui.h"
#include "apps/system/health/weight_entry_window.h"
#include "apps/system/health/weight_summary_card.h"

#include "test_health_app_includes.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void health_weight_entry_window_push(uint16_t initial_weight_dag,
                                     WeightEntrySavedCallback saved_callback, void *context) {
}

// Override fake_clock's PBL_WEAK default so the split layout's bin time renders
// in a stable 24h format.
bool clock_is_24h_style(void) {
  return true;
}

void test_health_activity_summary_card__initialize(void) {
  shell_prefs_set_units_distance(UnitsDistance_Miles);
  // Pin RTC to 2024-01-06 16:19:35 UTC (Saturday 16:19 -> bin minute 975 = 16:15).
  rtc_set_time(1704557975);

  // Setup graphics context
  framebuffer_init(&s_fb, &(GSize){DISP_COLS, DISP_ROWS});
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

static void prv_create_weight_summary_and_render(HealthData *health_data) {
  Window window;
  window_init(&window, WINDOW_NAME("Weight Summary"));
  Layer *window_layer = window_get_root_layer(&window);
  Layer *card_layer = health_weight_summary_card_create(health_data);
  layer_set_frame(card_layer, &window_layer->bounds);
  layer_add_child(window_layer, card_layer);
  window_set_background_color(&window, health_weight_summary_card_get_bg_color(card_layer));
  window_set_on_screen(&window, true, true);
  window_render(&window, &s_ctx);
  health_weight_summary_card_destroy(card_layer);
  window_deinit(&window);
}

static void prv_create_weight_detail_and_render(HealthData *health_data) {
  Window *window = health_weight_detail_card_create(health_data);
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
  window_set_on_screen(window, false, false);
  health_weight_detail_card_destroy(window);
}

static void prv_render_weight_period(uint8_t older_days, uint8_t newer_days) {
  const time_t now = rtc_get_time();
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {
      {.utc_sec = now - newer_days * SECONDS_PER_DAY, .weight_dag = 7700},
      {.utc_sec = now - older_days * SECONDS_PER_DAY, .weight_dag = 7740},
    },
    .weight_sample_count = 2,
  };
  prv_create_weight_detail_and_render(&health_data);
}

// Tests
//////////////////////

void test_health_activity_summary_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthData){});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_profile_only(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
  };
  prv_create_weight_summary_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_profile_metric(void) {
  shell_prefs_set_units_distance(UnitsDistance_KM);
  HealthData health_data = {
    .profile_weight_dag = 7730,
  };
  prv_create_weight_summary_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_entry_metric(void) {
  health_weight_entry_ui_draw(&s_ctx, &s_ctx.dest_bitmap.bounds, 773, 300, 2000, "ADD WEIGHT",
                              "kg", FONT_KEY_BITHAM_34_MEDIUM_NUMBERS);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_entry_imperial(void) {
  health_weight_entry_ui_draw(&s_ctx, &s_ctx.dest_bitmap.bounds, 1704, 661, 4409, "ADD WEIGHT",
                              "lb", FONT_KEY_BITHAM_34_MEDIUM_NUMBERS);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_latest_entry(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {{.utc_sec = 1704557975, .weight_dag = 7710}},
    .weight_sample_count = 1,
  };
  prv_create_weight_summary_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_summary_one_day_ago(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {{.utc_sec = 1704557975 - SECONDS_PER_DAY, .weight_dag = 7710}},
    .weight_sample_count = 1,
  };
  prv_create_weight_summary_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_summary_five_days_ago(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {{.utc_sec = 1704557975 - 5 * SECONDS_PER_DAY, .weight_dag = 7710}},
    .weight_sample_count = 1,
  };
  prv_create_weight_summary_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_one_entry(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {{.utc_sec = 1704557975, .weight_dag = 7710}},
    .weight_sample_count = 1,
  };
  prv_create_weight_detail_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_two_entries(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {
      {.utc_sec = 1704557975, .weight_dag = 7700},
      {.utc_sec = 1703953175, .weight_dag = 7660},
    },
    .weight_sample_count = 2,
  };
  prv_create_weight_detail_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_detail_empty(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
  };
  prv_create_weight_detail_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_history(void) {
  HealthData health_data = {
    .profile_weight_dag = 7730,
    .weight_samples = {
      {.utc_sec = 1704557975, .weight_dag = 7700},
      {.utc_sec = 1704385175, .weight_dag = 7685},
      {.utc_sec = 1704212375, .weight_dag = 7690},
      {.utc_sec = 1703953175, .weight_dag = 7680},
      {.utc_sec = 1703693975, .weight_dag = 7720},
      {.utc_sec = 1702829975, .weight_dag = 7710},
      {.utc_sec = 1702052375, .weight_dag = 7740},
    },
    .weight_sample_count = 7,
  };
  prv_create_weight_detail_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_period_yesterday_today(void) {
  prv_render_weight_period(1, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_period_five_days_today(void) {
  prv_render_weight_period(5, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_period_five_and_four_days(void) {
  prv_render_weight_period(5, 4);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_period_twenty_days_today(void) {
  prv_render_weight_period(20, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_period_fifty_days_today(void) {
  prv_render_weight_period(50, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_period_fifty_and_four_days(void) {
  prv_render_weight_period(50, 4);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__weight_no_recent_entries(void) {
  prv_render_weight_period(60, 50);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__no_current_steps(void) {
  HealthData health_data = {
    .step_data = 0,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 750,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical1(void) {
  HealthData health_data = {
    .step_data = 170,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 340,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical2(void) {
  HealthData health_data = {
    .step_data = 320,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 340,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical3(void) {
  HealthData health_data = {
    .step_data = 460,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 555,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical4(void) {
  HealthData health_data = {
    .step_data = 699,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 840,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_behind_typical5(void) {
  HealthData health_data = {
    .step_data = 837,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 914,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_equals_typical(void) {
  HealthData health_data = {
    .step_data = 837,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 837,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical1(void) {
  HealthData health_data = {
    .step_data = 340,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 170,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical2(void) {
  HealthData health_data = {
    .step_data = 400,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 379,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical3(void) {
  HealthData health_data = {
    .step_data = 780,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 480,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical4(void) {
  HealthData health_data = {
    .step_data = 866,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 700,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_typical5(void) {
  HealthData health_data = {
    .step_data = 970,
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 900,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_summary_card__render_current_above_expected(void) {
  HealthData health_data = {
    .step_data = {2000},
    .step_averages = {10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 50}, // 1000
    .current_step_average = 800,
    .step_average_last_updated_time = 975,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
