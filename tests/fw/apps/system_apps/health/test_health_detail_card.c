/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/health/health_detail_card.h"
#include "apps/system_apps/health/health_progress.h"

#include "test_health_app_includes.h"

#define BG_COLOR PBL_IF_COLOR_ELSE(GColorBlueMoon, GColorWhite)
#define FILL_COLOR PBL_IF_COLOR_ELSE(GColorKellyGreen, GColorDarkGray)
#define TODAY_FILL_COLOR PBL_IF_COLOR_ELSE(GColorMediumAquamarine, GColorDarkGray)

#define DEFAULT_ZONES { \
  { .label = "Today", .progress = 700, .fill_color = TODAY_FILL_COLOR, .hide_typical = true }, \
  { .label = "Wed", .progress = 1100, .fill_color = FILL_COLOR }, \
  { .label = "Tue", .progress = 400, .fill_color = FILL_COLOR }, \
  { .label = "Mon", .progress = 1300, .fill_color = FILL_COLOR }, \
  { .label = "Sun", .progress = 800, .fill_color = FILL_COLOR }, \
  { .label = "Sat", .progress = 700, .fill_color = FILL_COLOR }, \
  { .label = "Fri", .progress = 1200, .fill_color = FILL_COLOR }, \
}

static HealthDetailZone s_zones[] = DEFAULT_ZONES;


// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_health_detail_card__initialize(void) {
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

void test_health_detail_card__cleanup(void) {
}

// Helpers
//////////////////////

static Window* prv_create_card_and_render(HealthDetailCardConfig *config) {
  Window *window = (Window *)health_detail_card_create(config);
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
  return window;
}

// Tests
//////////////////////

void test_health_detail_card__render_no_data(void) {
  prv_create_card_and_render(&(HealthDetailCardConfig) {});
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_one_heading(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .bg_color = BG_COLOR,
  };

  prv_create_card_and_render(&config);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_two_headings(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .secondary_label = "LABEL2",
      .secondary_value = "value2",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .bg_color = BG_COLOR,
  };

  prv_create_card_and_render(&config);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_subtitle_text(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .secondary_label = "LABEL2",
      .secondary_value = "value2",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .num_subtitles = 1,
    .subtitles = &(HealthDetailSubtitle) {
      .label = "30 DAY AVG",
      .fill_color = PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack),
    },
    .bg_color = BG_COLOR,
  };

  prv_create_card_and_render(&config);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_no_subtitle(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .bg_color = BG_COLOR,
    .daily_avg = 900,
    .weekly_max = 1300,
    .num_zones = ARRAY_LENGTH(s_zones),
    .zones = s_zones,
  };

  prv_create_card_and_render(&config);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_zones(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .secondary_label = "LABEL2",
      .secondary_value = "value2",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .num_subtitles = 1,
    .subtitles = &(HealthDetailSubtitle) {
      .label = "30 DAY AVG",
      .fill_color = PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack),
    },
    .bg_color = BG_COLOR,
    .daily_avg = 900,
    .weekly_max = 1300,
    .num_zones = ARRAY_LENGTH(s_zones),
    .zones = s_zones,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&config);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 3), MenuRowAlignCenter, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_bg_and_zone_colors(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .num_subtitles = 1,
    .subtitles = &(HealthDetailSubtitle) {
      .label = "30 DAY AVG",
      .fill_color = PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack),
    },
    .bg_color = BG_COLOR,
    .daily_avg = 900,
    .weekly_max = 1300,
    .num_zones = ARRAY_LENGTH(s_zones),
    .zones = s_zones,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&config);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 2), MenuRowAlignCenter, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_crown(void) {
  HealthDetailZone zones[] = DEFAULT_ZONES;
  zones[1].show_crown = true;

  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .num_subtitles = 1,
    .subtitles = &(HealthDetailSubtitle) {
      .label = "30 DAY AVG",
      .fill_color = PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack),
    },
    .bg_color = BG_COLOR,
    .daily_avg = 900,
    .weekly_max = 1300,
    .num_zones = ARRAY_LENGTH(zones),
    .zones = zones,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&config);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 2), MenuRowAlignCenter, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__render_zone_hide_typical(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .bg_color = BG_COLOR,
    .daily_avg = 900,
    .weekly_max = 1300,
    .num_zones = ARRAY_LENGTH(s_zones),
    .zones = s_zones,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&config);

#if PBL_ROUND
  menu_layer_set_selected_index(&card->menu_layer, MenuIndex(0, 1), MenuRowAlignCenter, false);
#endif

  window_render(&card->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_health_detail_card__scroll_down(void) {
  HealthDetailCardConfig config = {
    .num_headings = 1,
    .headings = &(HealthDetailHeading) {
      .primary_label = "LABEL1",
      .primary_value = "value1",
      .fill_color = GColorWhite,
#if PBL_BW
      .outline_color = GColorBlack,
#endif
    },
    .bg_color = BG_COLOR,
    .daily_avg = 900,
    .weekly_max = 1300,
    .num_zones = ARRAY_LENGTH(s_zones),
    .zones = s_zones,
  };

  HealthDetailCard *card = (HealthDetailCard *)prv_create_card_and_render(&config);

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
