/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_health_app_includes.h"

#include <pbl/util/units.h>

#include <apps/system/health/data.h>
#include <apps/system/health/data_private.h>
#include <apps/system/health/detail_card.h>
#include <apps/system/health/hr_detail_card.h>

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;
static bool s_clock_24h;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

bool clock_is_24h_style(void) {
  return s_clock_24h;
}

static uint8_t s_today_bpm[PBL_MIN_PER_DAY];
static HealthHrChartData s_today;

void test_health_hr_detail_card__initialize(void) {
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

  s_clock_24h = false;
  memset(s_today_bpm, 0, sizeof(s_today_bpm));
  s_today = (HealthHrChartData){};
}

void test_health_hr_detail_card__cleanup(void) {
}

// Helpers
//////////////////////

static Window *prv_create_card_and_render(HealthData *health_data) {
  Window *window = (Window *)health_hr_detail_card_create(health_data, &s_today);
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
  return window;
}

static void prv_show_zones(HealthDetailCard *card) {
#if PBL_ROUND
  // The chart and headings pages come first, show the second zone
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 3), MenuRowAlignCenter, false);
#else
  // Page down once, as the down button would
  scroll_layer_set_content_offset(&card->scroll_layer, GPoint(0, -DISP_ROWS), false);
#endif
  window_render(&card->window, &s_ctx);
}

// Asleep until 7:00, a workout at 9:00 and readings every 10 minutes after; it is now 14:30
static void prv_fill_today_bpm(void) {
  for (int m = 3; m < 14 * 60 + 30; m += 10) {
    s_today_bpm[m] = ((m < 7 * 60) ? 54 : 72) + ((m * 37) % 7);
  }
  for (int m = 9 * 60; m < 9 * 60 + 40; m++) {
    s_today_bpm[m] = 100 + ((m - 9 * 60) * 3) / 2;
  }
  s_today = (HealthHrChartData){
    .minute_bpm = s_today_bpm,
    .num_minutes = 14 * 60 + 30,
  };
}

// Tests
//////////////////////

void test_health_hr_detail_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthData){});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_hr_detail_card__render_zones(void) {
  HealthData health_data = {
    .hr_zone1_minutes = 21,
    .hr_zone2_minutes = 13,
    .hr_zone3_minutes = 6,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);
  prv_show_zones(card);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_hr_detail_card__render_zones2(void) {
  HealthData health_data = {
    .hr_zone1_minutes = 30,
    .hr_zone2_minutes = 35,
    .hr_zone3_minutes = 40,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);
  prv_show_zones(card);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_hr_detail_card__render_chart(void) {
  HealthData health_data = {
    .hr_zone1_minutes = 21,
    .hr_zone2_minutes = 13,
    .hr_zone3_minutes = 6,
  };
  prv_fill_today_bpm();
  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_hr_detail_card__render_chart_then_zones(void) {
  HealthData health_data = {
    .hr_zone1_minutes = 21,
    .hr_zone2_minutes = 13,
    .hr_zone3_minutes = 6,
  };
  prv_fill_today_bpm();
  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);
  prv_show_zones(card);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_hr_detail_card__render_chart_24h(void) {
  s_clock_24h = true;
  prv_fill_today_bpm();
  prv_create_card_and_render(&(HealthData){});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_hr_detail_card__render_chart_just_after_midnight(void) {
  // A few readings within a couple of BPM of each other, so the axis is widened around them
  s_today_bpm[3] = 72;
  s_today_bpm[13] = 74;
  s_today_bpm[23] = 73;
  s_today = (HealthHrChartData){
    .minute_bpm = s_today_bpm,
    .num_minutes = 25,
  };
  prv_create_card_and_render(&(HealthData){});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
