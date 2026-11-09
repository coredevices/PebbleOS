/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sleep_detail_card.h"

#include "detail_card.h"

#include <stdio.h>

#include <pbl/i18n/i18n.h>
#include <pbl/services/activity/health_util.h>
#include <pbl/services/clock.h>
#include <pbl/util/units.h>

#include <applib/app_malloc.h>
#include <applib/pbl_std/pbl_std.h>

typedef struct HealthSleepDetailCard {
  HealthData *health_data;
  int32_t daily_avg;
  int32_t weekly_max;

  int16_t num_headings;
  HealthDetailHeading headings[MAX_NUM_HEADINGS];

  int16_t num_subtitles;
  HealthDetailSubtitle subtitles[MAX_NUM_SUBTITLES];

  int16_t num_zones;
  HealthDetailZone zones[MAX_NUM_ZONES];
} HealthSleepDetailCardData;

#ifdef HEALTH_DETAIL_TALL_RECT
#define HERO_INSET         (8)
#define HERO_CORNER_RADIUS (3)

//! LECO number fonts have no letters, so a 12h time with AM/PM falls back to Gothic
static GFont prv_hero_time_font(const char *text) {
  for (const char *c = text; *c; c++) {
    const char ch = *c;
    if (!((ch >= '0' && ch <= '9') || ch == ':' || ch == '-')) {
      return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    }
  }
  return fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
}

static void prv_draw_hero_stat_block(GContext *ctx, const GRect *block, GColor fill_color,
                                     const char *label, int32_t value_seconds, void *i18n_owner) {
  graphics_context_set_fill_color(ctx, fill_color);
  graphics_fill_round_rect(ctx, block, HERO_CORNER_RADIUS, GCornersAll);

  const GColor fg = gcolor_legible_over(fill_color);
  graphics_context_set_text_color(ctx, fg);
  const GRect label_rect = GRect(block->origin.x, block->origin.y + 5, block->size.w, 14);
  graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), label_rect,
                     GTextOverflowModeFill, GTextAlignmentCenter, nullptr);

  char value_buf[32];
  if (value_seconds > 0) {
    health_util_format_hours_and_minutes(value_buf, sizeof(value_buf), value_seconds, i18n_owner);
  } else {
    strncpy(value_buf, EN_DASH, sizeof(value_buf));
  }
  const GRect value_rect = GRect(block->origin.x, block->origin.y + 22, block->size.w, 30);
  graphics_draw_text(ctx, value_buf, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), value_rect,
                     GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
}

//! Hero page for tall rectangular displays: session time range, week chart, stat blocks.
//! The per-day zone list starts below the display and scrolls in as the second page.
static void prv_draw_hero(GContext *ctx, const GRect *bounds, void *context) {
  HealthSleepDetailCardData *card_data = context;
  HealthData *health_data = card_data->health_data;
  const int16_t w = bounds->size.w;
  const GColor fg = gcolor_legible_over(PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorWhite));

  const GRect caption_rect = GRect(0, 4, w, 20);
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, pbl_i18n_get("SLEEP SESSION", card_data),
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), caption_rect,
                     GTextOverflowModeFill, GTextAlignmentCenter, nullptr);

  const GRect time_rect = GRect(0, 26, w, 40);
  char time_buf[24];
  const int32_t sleep_start = health_data_sleep_get_start_time(health_data);
  const int32_t sleep_end = health_data_sleep_get_end_time(health_data);
  if (sleep_start > 0 && sleep_end > 0 && sleep_start != sleep_end) {
    int pos = 0;
    pos += clock_format_time(time_buf + pos, sizeof(time_buf) - pos, sleep_start / PBL_SEC_PER_HOUR,
                             (sleep_start % PBL_SEC_PER_HOUR) / PBL_SEC_PER_MIN, false);
    pos += snprintf(time_buf + pos, sizeof(time_buf) - pos, "-");
    pos += clock_format_time(time_buf + pos, sizeof(time_buf) - pos, sleep_end / PBL_SEC_PER_HOUR,
                             (sleep_end % PBL_SEC_PER_HOUR) / PBL_SEC_PER_MIN, false);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, time_buf, prv_hero_time_font(time_buf), time_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
  } else {
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, EN_DASH, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), time_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
  }

  // Week bar chart, one bar per day
  const int16_t chart_top = 76;
  const int16_t chart_base = 130;
  const int16_t gap = 4;
  const int16_t bar_w =
      (w - 2 * HERO_INSET - (card_data->num_zones - 1) * gap) / card_data->num_zones;
  const int32_t max_progress = MAX(card_data->weekly_max, card_data->daily_avg) * 11 / 10;
  for (int i = 0; i < card_data->num_zones; i++) {
    const HealthDetailZone *zone = &card_data->zones[i];
    const int16_t x = HERO_INSET + i * (bar_w + gap);
    int16_t bar_h = 0;
    if (max_progress > 0) {
      bar_h = (chart_base - chart_top) * zone->progress / max_progress;
    }
    if (bar_h < 3) {
      bar_h = 3;
    }
    graphics_context_set_fill_color(ctx, zone->fill_color);
    const GRect bar_rect = GRect(x, chart_base - bar_h, bar_w, bar_h);
    graphics_fill_rect(ctx, &bar_rect);

    char day_letter[2] = {zone->label[0], '\0'};
    const GRect letter_rect = GRect(x, chart_base + 3, bar_w, 14);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, day_letter, fonts_get_system_font(FONT_KEY_GOTHIC_14), letter_rect,
                       GTextOverflowModeFill, GTextAlignmentCenter, nullptr);
  }

  // Stat blocks: deep sleep and 30 day average
  const int16_t block_y = 154;
  const int16_t block_h = 64;
  const int16_t block_gap = 8;
  const int16_t block_w = (w - 2 * HERO_INSET - block_gap) / 2;
  const GRect left_block = GRect(HERO_INSET, block_y, block_w, block_h);
  const GRect right_block = GRect(HERO_INSET + block_w + block_gap, block_y, block_w, block_h);
  prv_draw_hero_stat_block(ctx, &left_block, PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorWhite),
                           pbl_i18n_get("DEEP SLEEP", card_data),
                           health_data_current_deep_sleep_get(health_data), card_data);
  prv_draw_hero_stat_block(ctx, &right_block, PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack),
                           pbl_i18n_get("30 DAY", card_data), card_data->daily_avg, card_data);
}
#endif // HEALTH_DETAIL_TALL_RECT

#ifndef HEALTH_DETAIL_TALL_RECT
static void prv_set_sleep_session(char *buffer, size_t buffer_size, int32_t sleep_start,
                                  int32_t sleep_end) {
  // We don't have a sleep session if either start or end time is not greater than 0.
  // Sometimes if there's no sleep, sleep session rendered as "16:00 - 16:00" so for a quick
  // fix, we assume there's no sleep if the start and end times are the same.
  if (sleep_start <= 0 || sleep_end <= 0 || sleep_start == sleep_end) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }

  const int start_hours = sleep_start / PBL_SEC_PER_HOUR;
  const int start_minutes = (sleep_start % PBL_SEC_PER_HOUR) / PBL_SEC_PER_MIN;
  const int end_hours = sleep_end / PBL_SEC_PER_HOUR;
  const int end_minutes = (sleep_end % PBL_SEC_PER_HOUR) / PBL_SEC_PER_MIN;

  int pos = 0;

  pos += clock_format_time(buffer + pos, buffer_size - pos, start_hours, start_minutes, false);

  pos += snprintf(buffer + pos, buffer_size - pos, " %s ", "-");

  pos += clock_format_time(buffer + pos, buffer_size - pos, end_hours, end_minutes, false);
}

static void prv_set_deep_sleep(char *buffer, size_t buffer_size, int32_t sleep_duration,
                               void *i18n_owner) {
  if (sleep_duration <= 0) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }

  health_util_format_hours_and_minutes(buffer, buffer_size, sleep_duration, i18n_owner);
}

static void prv_set_avg(char *buffer, size_t buffer_size, int32_t daily_avg, void *i18n_owner) {
#if PBL_ROUND
  int avg_len = snprintf(buffer, buffer_size, "%s\n", pbl_i18n_get("30 DAY AVG", i18n_owner));
#else
  int avg_len = snprintf(buffer, buffer_size, "%s ", pbl_i18n_get("30 DAY", i18n_owner));
#endif

  if (daily_avg <= 0) {
    strncpy(buffer + avg_len, EN_DASH, buffer_size - avg_len);
  } else {
    health_util_format_hours_and_minutes(buffer + avg_len, buffer_size - avg_len, daily_avg,
                                         i18n_owner);
  }
}
#endif // !HEALTH_DETAIL_TALL_RECT

Window *health_sleep_detail_card_create(HealthData *health_data) {
  HealthSleepDetailCardData *card_data = app_zalloc_check(sizeof(HealthSleepDetailCardData));

  card_data->health_data = health_data;
  card_data->daily_avg = health_data_sleep_get_monthly_average(health_data);

  const GColor fill_color = PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorDarkGray);
  const GColor today_fill_color = PBL_IF_COLOR_ELSE(GColorElectricBlue, GColorDarkGray);

  health_detail_card_set_render_day_zones(
      card_data->zones, &card_data->num_zones, &card_data->weekly_max,
      true /* format hours and minutes */, false /* show crown */, fill_color, today_fill_color,
      health_data_sleep_get(health_data), card_data);

#ifdef HEALTH_DETAIL_TALL_RECT
  // Hero page up top; the per-day zone list starts below the display and scrolls in as page 2
  const HealthDetailCardConfig config = {
    .daily_avg = card_data->daily_avg,
    .weekly_max = card_data->weekly_max,
    .bg_color = PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorWhite),
    .num_zones = card_data->num_zones,
    .zones = card_data->zones,
    .header =
        &(HealthDetailHeader){
          .height = PBL_DISPLAY_HEIGHT,
          .draw = prv_draw_hero,
          .context = card_data,
        },
    .data = card_data,
  };

  return (Window *)health_detail_card_create(&config);
#else
  const size_t buffer_len = 32;

  HealthDetailHeading *heading = &card_data->headings[card_data->num_headings++];

  *heading = (HealthDetailHeading){
    .primary_label = (char *)pbl_i18n_get("SLEEP SESSION", card_data),
    .primary_value = app_zalloc_check(buffer_len),
    .fill_color = GColorWhite,
    .outline_color = PBL_IF_COLOR_ELSE(GColorClear, GColorBlack),
  };

  prv_set_sleep_session(heading->primary_value, buffer_len,
                        health_data_sleep_get_start_time(health_data),
                        health_data_sleep_get_end_time(health_data));

  heading = &card_data->headings[card_data->num_headings++];

  *heading = (HealthDetailHeading){
    .primary_label = (char *)pbl_i18n_get("DEEP SLEEP", card_data),
    .primary_value = app_zalloc_check(buffer_len),
    .fill_color = PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorWhite),
#if PBL_BW
    .outline_color = GColorBlack,
#endif
  };

  prv_set_deep_sleep(heading->primary_value, buffer_len,
                     health_data_current_deep_sleep_get(health_data), card_data);

  HealthDetailSubtitle *subtitle = &card_data->subtitles[card_data->num_subtitles++];

  *subtitle = (HealthDetailSubtitle){
    .label = app_zalloc_check(buffer_len),
    .fill_color = PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack),
  };

  prv_set_avg(subtitle->label, buffer_len, card_data->daily_avg, card_data);

  const HealthDetailCardConfig config = {
    .num_headings = card_data->num_headings,
    .headings = card_data->headings,
    .num_subtitles = card_data->num_subtitles,
    .subtitles = card_data->subtitles,
    .daily_avg = card_data->daily_avg,
    .weekly_max = card_data->weekly_max,
    .bg_color = PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorWhite),
    .num_zones = card_data->num_zones,
    .zones = card_data->zones,
    .data = card_data,
  };

  return (Window *)health_detail_card_create(&config);
#endif // HEALTH_DETAIL_TALL_RECT
}

void health_sleep_detail_card_destroy(Window *window) {
  HealthDetailCard *card = (HealthDetailCard *)window;
  HealthSleepDetailCardData *card_data = card->data;
  for (int i = 0; i < card_data->num_headings; i++) {
    app_free(card_data->headings[i].primary_value);
    app_free(card_data->headings[i].secondary_value);
  }
  for (int i = 0; i < card_data->num_subtitles; i++) {
    app_free(card_data->subtitles[i].label);
  }
  for (int i = 0; i < card_data->num_zones; i++) {
    app_free(card_data->zones[i].label);
  }
  pbl_i18n_free_all(card_data);
  app_free(card_data);
  health_detail_card_destroy(card);
}
