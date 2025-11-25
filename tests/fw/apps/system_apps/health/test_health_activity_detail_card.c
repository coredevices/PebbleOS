/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/health/health_activity_detail_card.h"
#include "apps/system_apps/health/health_detail_card.h"
#include "apps/system_apps/health/health_data.h"
#include "apps/system_apps/health/health_data_private.h"
#include "apps/system_apps/health/health_progress.h"

#include "test_health_app_includes.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_health_activity_detail_card__initialize(void) {
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

void test_health_activity_detail_card__cleanup(void) {
}

// Helpers
//////////////////////

static Window* prv_create_card_and_render(HealthData *health_data) {
  Window *window = health_activity_detail_card_create(health_data);
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
  return window;
}

// Tests
//////////////////////

void test_health_activity_detail_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthData) {});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_detail_card__render_current_calories_and_distance(void) {
  HealthData health_data = {
    .current_calories = 123,
    .current_distance_meters = 4000,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_detail_card__render_no_calories(void) {
  HealthData health_data = {
    .current_calories = 0,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_detail_card__render_no_distance(void) {
  HealthData health_data = {
    .current_distance_meters = 0,
  };

  prv_create_card_and_render(&health_data);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_detail_card__render_step_data(void) {
  HealthData health_data = {
    .step_data = {600, 900, 700, 1200, 1400, 1300, 1000},
    .monthly_step_average = 1000,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 3), MenuRowAlignCenter, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_activity_detail_card__render_day_label_no_steps(void) {
  HealthData health_data = {
    .step_data = {600, 0, 700},
    .monthly_step_average = 1000,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&health_data);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 2), MenuRowAlignCenter, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
