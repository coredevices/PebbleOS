/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "health_activity_detail_card.h"
#include "health_detail_card.h"
#include "services/normal/activity/health_util.h"

#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"

#include <stdio.h>

typedef struct HealthActivityDetailCard {
  int32_t daily_avg;
  int32_t weekly_max;

  int16_t num_headings;
  HealthDetailHeading headings[MAX_NUM_HEADINGS];

  int16_t num_subtitles;
  HealthDetailSubtitle subtitles[MAX_NUM_SUBTITLES];

  int16_t num_zones;
  HealthDetailZone zones[MAX_NUM_ZONES];
} HealthActivityDetailCardData;

static void prv_set_calories(char *buffer, size_t buffer_size, int32_t current_calories) {
  if (current_calories == 0) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }

  snprintf(buffer, buffer_size, "%"PRId32, current_calories);
}

static void prv_set_distance(char *buffer, size_t buffer_size, int32_t current_distance_meters) {
  if (current_distance_meters == 0) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }

  const int conversion_factor = health_util_get_distance_factor();
  const char *units_string = health_util_get_distance_string(i18n_noop("MI"), i18n_noop("KM"));

  char distance_buffer[HEALTH_WHOLE_AND_DECIMAL_LENGTH];
  health_util_format_whole_and_decimal(distance_buffer, HEALTH_WHOLE_AND_DECIMAL_LENGTH,
                                       current_distance_meters, conversion_factor);

  snprintf(buffer, buffer_size, "%s%s", distance_buffer, units_string);
}

static void prv_set_avg(char *buffer, size_t buffer_size, int32_t daily_avg, void *i18n_owner) {
  int pos = 0;

  pos += snprintf(buffer, buffer_size,
                  PBL_IF_ROUND_ELSE("%s\n", "%s"), i18n_get("30 DAY AVG", i18n_owner));

  if (daily_avg > 0) {
    snprintf(buffer + pos, buffer_size - pos, " %"PRId32, daily_avg);
  } else {
    snprintf(buffer + pos, buffer_size - pos, " "EN_DASH);
  }
}

Window *health_activity_detail_card_create(HealthData *health_data) {
  HealthActivityDetailCardData *card_data = app_zalloc_check(sizeof(HealthActivityDetailCardData));

  card_data->daily_avg = health_data_steps_get_monthly_average(health_data);

  const GColor fill_color = PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorDarkGray);
  const GColor today_fill_color = PBL_IF_COLOR_ELSE(GColorScreaminGreen, GColorDarkGray);

  health_detail_card_set_render_day_zones(card_data->zones,
                                          &card_data->num_zones,
                                          &card_data->weekly_max,
                                          false /* format hours and minutes */,
                                          true /* show crown */,
                                          fill_color,
                                          today_fill_color,
                                          health_data_steps_get(health_data),
                                          card_data);

  const size_t buffer_len = 32;

  HealthDetailHeading *heading = &card_data->headings[card_data->num_headings++];

  *heading = (HealthDetailHeading) {
    .primary_label = (char *)i18n_get("CALORIES", card_data),
    .primary_value = app_zalloc_check(buffer_len),
    .secondary_label = (char *)i18n_get("DISTANCE", card_data),
    .secondary_value = app_zalloc_check(buffer_len),
    .fill_color = GColorWhite,
    .outline_color = PBL_IF_COLOR_ELSE(GColorClear, GColorBlack),
  };

  prv_set_calories(heading->primary_value, buffer_len,
                   health_data_current_calories_get(health_data));

  prv_set_distance(heading->secondary_value, buffer_len,
                   health_data_current_distance_meters_get(health_data));

  HealthDetailSubtitle *subtitle = &card_data->subtitles[card_data->num_subtitles++];

  *subtitle = (HealthDetailSubtitle) {
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
    .bg_color = PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite),
    .num_zones = card_data->num_zones,
    .zones = card_data->zones,
    .data = card_data,
  };

  return (Window *)health_detail_card_create(&config);
}

void health_activity_detail_card_destroy(Window *window) {
  HealthDetailCard *card = (HealthDetailCard *)window;
  HealthActivityDetailCardData *card_data = card->data;
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
  i18n_free_all(card_data);
  app_free(card_data);
  health_detail_card_destroy(card);
}
