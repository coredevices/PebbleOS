/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/health/health_data.h"
#include "apps/system_apps/health/health_data_private.h"
#include "apps/system_apps/health/health_sleep_summary_card.h"

#include "test_health_app_includes.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

const int s_now_utc = 1467763200; // July 6, 2016

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_health_sleep_summary_card__initialize(void) {
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

void test_health_sleep_summary_card__cleanup(void) {
}

// Helpers
//////////////////////

static void prv_create_card_and_render(HealthData *health_data) {
  Window window;
  window_init(&window, WINDOW_NAME("Health"));
  Layer *window_layer = window_get_root_layer(&window);
  Layer *card_layer = health_sleep_summary_card_create(health_data);
  layer_set_frame(card_layer, &window_layer->bounds);
  layer_add_child(window_layer, card_layer);
  window_set_background_color(&window, health_sleep_summary_card_get_bg_color(card_layer));
  window_set_on_screen(&window, true, true);
  window_render(&window, &s_ctx);
}

// Tests
//////////////////////

void test_health_sleep_summary_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthData) {});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_summary_card__render_no_typical(void) {
  HealthData health_data = {
    .monthly_sleep_average = (9 * SECONDS_PER_HOUR),
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_summary_card__render_no_sleep_last_night(void) {
  HealthData health_data = {
    // Used for text
    .typical_sleep = (10 * SECONDS_PER_HOUR),
    .monthly_sleep_average = (300 * SECONDS_PER_HOUR),

    // Used for typical
    .typical_sleep_start = -4 * SECONDS_PER_HOUR,
    .typical_sleep_end = 7 * SECONDS_PER_HOUR,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_summary_card__render_sleep_late_start_early_end1(void) {
  const time_t start_of_today = time_util_get_midnight_of(s_now_utc);

  HealthData health_data = {
    // Used for text
    .sleep_data[0] = (8 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .typical_sleep = (10 * SECONDS_PER_HOUR),
    .monthly_sleep_average = (300 * SECONDS_PER_HOUR),

    // Used for typical
    .sleep_start = -3 * SECONDS_PER_HOUR,
    .sleep_end = 5 * SECONDS_PER_HOUR,
    .typical_sleep_start = -4 * SECONDS_PER_HOUR,
    .typical_sleep_end = 7 * SECONDS_PER_HOUR,

    // The sleep ring are filled by sleep sessions
    .num_activity_sessions = 3,
    .activity_sessions[0] = {
      .start_utc = start_of_today - (3 * SECONDS_PER_HOUR), // 9pm
      .length_min = (3 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    },
    .activity_sessions[1] = {
      .start_utc = start_of_today + (1 * SECONDS_PER_HOUR), // 1am
      .length_min = (4 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    },
    .activity_sessions[2] = {
      .start_utc = start_of_today + (2 * SECONDS_PER_HOUR), // 2am
      .length_min = (1 * MINUTES_PER_HOUR) + 30,
      .type = ActivitySessionType_RestfulSleep,
    },
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_summary_card__render_sleep_late_start_early_end2(void) {
  const time_t start_of_today = time_util_get_midnight_of(s_now_utc);

  HealthData health_data = {
    // Used for text
    .sleep_data[0] = (8 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .typical_sleep = (10 * SECONDS_PER_HOUR),
    .monthly_sleep_average = (300 * SECONDS_PER_HOUR),

    // Used for typical
    .sleep_start = 0 * SECONDS_PER_HOUR,
    .sleep_end = 7 * SECONDS_PER_HOUR,
    .typical_sleep_start = -1 * SECONDS_PER_HOUR,
    .typical_sleep_end = 8 * SECONDS_PER_HOUR,

    // The sleep ring are filled by sleep sessions
    .num_activity_sessions = 3,
    .activity_sessions[0] = {
      .start_utc = start_of_today - (0 * SECONDS_PER_HOUR), // 12am
      .length_min = (7 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    },
    .activity_sessions[1] = {
      .start_utc = start_of_today + (2 * SECONDS_PER_HOUR), // 2am
      .length_min = (1 * MINUTES_PER_HOUR) + 40,
      .type = ActivitySessionType_RestfulSleep,
    },
    .activity_sessions[2] = {
      .start_utc = start_of_today + (4 * SECONDS_PER_HOUR), // 4am
      .length_min = (2 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_RestfulSleep,
    },
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_summary_card__render_sleep_early_start_early_end1(void) {
  const time_t start_of_today = time_util_get_midnight_of(s_now_utc);

  HealthData health_data = {
    // Used for text
    .sleep_data[0] = (8 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .typical_sleep = (10 * SECONDS_PER_HOUR),
    .monthly_sleep_average = (300 * SECONDS_PER_HOUR),

    // Used for typical
    .sleep_start = -3 * SECONDS_PER_HOUR,
    .sleep_end = 7 * SECONDS_PER_HOUR,
    .typical_sleep_start = -1 * SECONDS_PER_HOUR,
    .typical_sleep_end = 8 * SECONDS_PER_HOUR,

    // The sleep ring are filled by sleep sessions
    .num_activity_sessions = 3,
    .activity_sessions[0] = {
      .start_utc = start_of_today - (3 * SECONDS_PER_HOUR), // 9pm
      .length_min = (10 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    },
    .activity_sessions[1] = {
      .start_utc = start_of_today - (2 * SECONDS_PER_HOUR), // 10pm
      .length_min = (1 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_RestfulSleep,
    },
    .activity_sessions[2] = {
      .start_utc = start_of_today + (3 * SECONDS_PER_HOUR), // 3am
      .length_min = (1 * MINUTES_PER_HOUR) + 15,
      .type = ActivitySessionType_RestfulSleep,
    },
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_summary_card__render_sleep_early_start_late_end1(void) {
  const time_t start_of_today = time_util_get_midnight_of(s_now_utc);

  HealthData health_data = {
    // Used for text
    .sleep_data[0] = (8 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .typical_sleep = (10 * SECONDS_PER_HOUR),
    .monthly_sleep_average = (300 * SECONDS_PER_HOUR),

    // Used for typical
    .sleep_start = -3 * SECONDS_PER_HOUR,
    .sleep_end = 7 * SECONDS_PER_HOUR,
    .typical_sleep_start = -4 * SECONDS_PER_HOUR,
    .typical_sleep_end = 5 * SECONDS_PER_HOUR,

    // The sleep ring are filled by sleep sessions
    .num_activity_sessions = 2,
    .activity_sessions[0] = {
      .start_utc = start_of_today - (3 * SECONDS_PER_HOUR),
      .length_min = (3 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    },
    .activity_sessions[1] = {
      .start_utc = start_of_today + (1 * SECONDS_PER_HOUR),
      .length_min = (6 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    },
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}


void test_health_sleep_summary_card__render_sleep_late_start_late_end1(void) {
  const time_t start_of_today = time_util_get_midnight_of(s_now_utc);

  HealthData health_data = {
    // Used for text
    .sleep_data[0] = (8 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .typical_sleep = (10 * SECONDS_PER_HOUR),
    .monthly_sleep_average = (300 * SECONDS_PER_HOUR),

    // Used for typical
    .sleep_start = -4 * SECONDS_PER_HOUR,
    .sleep_end = 4 * SECONDS_PER_HOUR,
    .typical_sleep_start = -3 * SECONDS_PER_HOUR,
    .typical_sleep_end = 3 * SECONDS_PER_HOUR,

    // The sleep ring are filled by sleep sessions
    .num_activity_sessions = 1,
    .activity_sessions[0] = {
      .start_utc = start_of_today - (4 * SECONDS_PER_HOUR),
      .length_min = (8 * MINUTES_PER_HOUR),
      .type = ActivitySessionType_Sleep,
    }
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
