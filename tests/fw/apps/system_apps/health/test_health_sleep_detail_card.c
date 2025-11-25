/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/health/health_sleep_detail_card.h"
#include "apps/system_apps/health/health_detail_card.h"
#include "apps/system_apps/health/health_data.h"
#include "apps/system_apps/health/health_data_private.h"

#include "test_health_app_includes.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_health_sleep_detail_card__initialize(void) {
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

void test_health_sleep_detail_card__cleanup(void) {
}

// Helpers
//////////////////////

// static void prv_create_card_and_render(HealthData *health_data) {
//   Window *window = health_sleep_detail_card_create(health_data);
//   window_set_on_screen(window, true, true);
//   window_render(window, &s_ctx);
// }

static Window* prv_create_card_and_render(HealthData *health_data) {
  Window *window = (Window *)health_sleep_detail_card_create(health_data);
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
  return window;
}

// Tests
//////////////////////

void test_health_sleep_detail_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthData) {});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_detail_card__render_sleep_session(void) {
  HealthData health_data = {
    .sleep_start = (23 * SECONDS_PER_HOUR) + (3 * SECONDS_PER_MINUTE),
    .sleep_end = (7 * SECONDS_PER_HOUR) + (45 * SECONDS_PER_MINUTE),
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_detail_card__render_sleep_session_same_start_end_time(void) {
  HealthData health_data = {
    .sleep_start = (16 * SECONDS_PER_HOUR),
    .sleep_end = (16 * SECONDS_PER_HOUR),
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_detail_card__render_30_day_avg(void) {
  HealthData health_data = {
    .monthly_sleep_average = (8 * SECONDS_PER_HOUR) + (17 * SECONDS_PER_MINUTE),
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_detail_card__render_deep_sleep(void) {
  HealthData health_data = {
    .deep_sleep = (3 * SECONDS_PER_HOUR) + (23 * SECONDS_PER_MINUTE),
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_detail_card__render_sleep_data_1(void) {
  HealthData health_data = {
    .sleep_data[0] = (7 * SECONDS_PER_HOUR) + (11 * SECONDS_PER_MINUTE),
    .sleep_data[1] = (6 * SECONDS_PER_HOUR) + (52 * SECONDS_PER_MINUTE),
    .sleep_data[2] = (7 * SECONDS_PER_HOUR) + (13 * SECONDS_PER_MINUTE),
    .sleep_data[3] = (9 * SECONDS_PER_HOUR) + (21 * SECONDS_PER_MINUTE),
    .sleep_data[4] = (9 * SECONDS_PER_HOUR) + (18 * SECONDS_PER_MINUTE),
    .monthly_sleep_average = (8 * SECONDS_PER_HOUR) + (17 * SECONDS_PER_MINUTE),
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 1), MenuRowAlignCenter, false);
#else
  GPoint offset = scroll_layer_get_content_offset(&card->scroll_layer);

  // scroll past sleep session, deep sleep and avg
  offset.y -= 114;

  scroll_layer_set_content_offset(&card->scroll_layer, offset, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_sleep_detail_card__render_sleep_data_2(void) {
  HealthData health_data = {
    .sleep_data[0] = (7 * SECONDS_PER_HOUR) + (14 * SECONDS_PER_MINUTE),
    .sleep_data[1] = (4 * SECONDS_PER_HOUR) + (59 * SECONDS_PER_MINUTE),
    .sleep_data[2] = (8 * SECONDS_PER_HOUR) + (17 * SECONDS_PER_MINUTE),
    .sleep_data[3] = (5 * SECONDS_PER_HOUR) + (34 * SECONDS_PER_MINUTE),
    .sleep_data[4] = (7 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .sleep_data[5] = (8 * SECONDS_PER_HOUR) + (12 * SECONDS_PER_MINUTE),
    .sleep_data[6] = (10 * SECONDS_PER_HOUR) + (11 * SECONDS_PER_MINUTE),
    .monthly_sleep_average = (8 * SECONDS_PER_HOUR) + (36 * SECONDS_PER_MINUTE),
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 7), MenuRowAlignCenter, false);
#else
  GPoint offset = scroll_layer_get_content_offset(&card->scroll_layer);
  offset.y -= scroll_layer_get_content_size(&card->scroll_layer).h;

  scroll_layer_set_content_offset(&card->scroll_layer, offset, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
