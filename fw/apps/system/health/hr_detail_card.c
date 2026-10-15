/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hr_detail_card.h"

#include "detail_card.h"
#include "hr_chart.h"

#include <stdio.h>

#include <pbl/services/activity/health_util.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/util/units.h>

#include <applib/fonts/codepoint.h>
#include <board/display.h>
#include <kernel/pbl_malloc.h>

typedef struct HealthHrDetailCard {
  int16_t num_headings;
  HealthDetailHeading headings[MAX_NUM_HEADINGS];

  int16_t num_zones;
  HealthDetailZone zones[MAX_NUM_ZONES];

  HealthHrChart chart;
} HealthHrDetailCardData;

#define DEFAULT_MAX_PROGRESS (10 * PBL_SEC_PER_MIN)
#define CHART_HEIGHT         PBL_IF_RECT_ELSE((DISP_ROWS * 4) / 9, (DISP_ROWS * 2) / 5)

static void prv_draw_chart(GContext *ctx, const GRect *bounds, void *context) {
  HealthHrDetailCardData *card_data = context;
  health_hr_chart_draw(ctx, bounds, &card_data->chart);
}

static void prv_set_range_value(char *buffer, const size_t buffer_size, const HealthHrChart *chart,
                                void *i18n_owner) {
  if (chart->latest_minute < 0) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }
  /// Range of heart rate readings, e.g. "52–104 BPM"
  snprintf(buffer, buffer_size, i18n_get("%d–%d BPM", i18n_owner), chart->min_bpm, chart->max_bpm);
}

static void prv_set_zone(HealthDetailZone *zone, int32_t minutes, int32_t *max_progress,
                         const size_t buffer_size, const char *zone_label, void *i18n_owner) {
  *zone = (HealthDetailZone){
    .label = app_zalloc_check(buffer_size),
    .progress = minutes * PBL_SEC_PER_MIN,
    .fill_color = PBL_IF_COLOR_ELSE(GColorSunsetOrange, GColorDarkGray),
  };

  int pos = snprintf(zone->label, buffer_size, "%s ", i18n_get(zone_label, i18n_owner));
  if (zone->progress) {
    health_util_format_hours_and_minutes(zone->label + pos, buffer_size - pos, zone->progress,
                                         i18n_owner);
  }

  if (zone->progress > *max_progress) {
    *max_progress = zone->progress;
  }
}

static void prv_set_heading_value(char *buffer, const size_t buffer_size, const int32_t zone_time_s,
                                  void *i18n_owner) {
  if (zone_time_s == 0) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }

  health_util_format_hours_and_minutes(buffer, buffer_size, zone_time_s, i18n_owner);
}

Window *health_hr_detail_card_create(HealthData *health_data, const HealthHrChartData *today) {
  HealthHrDetailCardData *card_data = app_zalloc_check(sizeof(HealthHrDetailCardData));

  const int32_t zone1_minutes = health_data_hr_get_zone1_minutes(health_data);
  const int32_t zone2_minutes = health_data_hr_get_zone2_minutes(health_data);
  const int32_t zone3_minutes = health_data_hr_get_zone3_minutes(health_data);
  const int32_t zone_time_minutes = zone1_minutes + zone2_minutes + zone3_minutes;

  int32_t max_progress = DEFAULT_MAX_PROGRESS;

  const size_t buffer_size = 32;

  prv_set_zone(&card_data->zones[card_data->num_zones++], zone1_minutes, &max_progress, buffer_size,
               i18n_noop("Fat Burn"), card_data);

  prv_set_zone(&card_data->zones[card_data->num_zones++], zone2_minutes, &max_progress, buffer_size,
               i18n_noop("Endurance"), card_data);

  prv_set_zone(&card_data->zones[card_data->num_zones++], zone3_minutes, &max_progress, buffer_size,
               i18n_noop("Performance"), card_data);

  health_hr_chart_init(&card_data->chart, today);

  HealthDetailHeading *range_heading = &card_data->headings[card_data->num_headings++];
  *range_heading = (HealthDetailHeading){
    /// Heading above a chart of today's heart rate and its lowest to highest reading
    .primary_label = (char *)i18n_get("TODAY'S RANGE", card_data),
    .primary_value = app_zalloc_check(buffer_size),
    // One block with the chart's box
    .fill_color = HEALTH_HR_CHART_BG_COLOR,
    .outline_color = GColorClear,
  };
  prv_set_range_value(range_heading->primary_value, buffer_size, &card_data->chart, card_data);

  HealthDetailHeading *heading = &card_data->headings[card_data->num_headings++];

  *heading = (HealthDetailHeading){
    /// Heading above the time spent in each heart rate zone today
    .primary_label = (char *)i18n_get("TIME IN ZONES", card_data),
    .primary_value = app_zalloc_check(buffer_size),
    .fill_color = PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorWhite),
    .outline_color = PBL_IF_COLOR_ELSE(GColorClear, GColorBlack),
  };

  prv_set_heading_value(heading->primary_value, buffer_size, (zone_time_minutes * PBL_SEC_PER_MIN),
                        card_data);

  const HealthDetailChart chart = {
    .height = CHART_HEIGHT,
    .bg_color = HEALTH_HR_CHART_BG_COLOR,
    .draw = prv_draw_chart,
    .context = card_data,
  };

  const HealthDetailCardConfig config = {
    .num_headings = card_data->num_headings,
    .headings = card_data->headings,
    .weekly_max = max_progress,
    .bg_color = GColorWhite,
    .num_zones = card_data->num_zones,
    .zones = card_data->zones,
    .chart = &chart,
    .data = card_data,
  };

  return (Window *)health_detail_card_create(&config);
}

void health_hr_detail_card_destroy(Window *window) {
  HealthDetailCard *card = (HealthDetailCard *)window;
  HealthHrDetailCardData *card_data = card->data;
  for (int i = 0; i < card_data->num_headings; i++) {
    app_free(card_data->headings[i].primary_value);
  }
  for (int i = 0; i < card_data->num_zones; i++) {
    app_free(card_data->zones[i].label);
  }
  health_hr_chart_deinit(&card_data->chart);
  i18n_free_all(card_data);
  app_free(card_data);
  health_detail_card_destroy(card);
}
