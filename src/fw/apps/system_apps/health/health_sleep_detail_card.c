/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "health_sleep_detail_card.h"
#include "health_detail_card.h"
#include "services/normal/activity/health_util.h"

#include "kernel/pbl_malloc.h"
#include "services/common/clock.h"
#include "services/common/i18n/i18n.h"

#include <stdio.h>

typedef struct HealthSleepDetailCard {
  int32_t daily_avg;
  int32_t weekly_max;

  int16_t num_headings;
  HealthDetailHeading headings[MAX_NUM_HEADINGS];

  int16_t num_subtitles;
  HealthDetailSubtitle subtitles[MAX_NUM_SUBTITLES];

  int16_t num_zones;
  HealthDetailZone zones[MAX_NUM_ZONES];
} HealthSleepDetailCardData;

static void prv_set_sleep_session(char *buffer, size_t buffer_size, int32_t sleep_start,
                                  int32_t sleep_end) {
  // We don't have a sleep session if either start or end time is not greater than 0.
  // Sometimes if there's no sleep, sleep session rendered as "16:00 - 16:00" so for a quick
  // fix, we assume there's no sleep if the start and end times are the same.
  // https://pebbletechnology.atlassian.net/browse/PBL-40031
  if (sleep_start <= 0 || sleep_end <= 0 || sleep_start == sleep_end) {
    strncpy(buffer, EN_DASH, buffer_size);
    return;
  }

  const int start_hours = sleep_start / SECONDS_PER_HOUR;
  const int start_minutes = (sleep_start % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
  const int end_hours = sleep_end / SECONDS_PER_HOUR;
  const int end_minutes = (sleep_end % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;

  int pos = 0;

  pos += clock_format_time(buffer + pos, buffer_size - pos,
                           start_hours, start_minutes, false);

  pos += snprintf(buffer + pos, buffer_size - pos, " %s ", "-");

  pos += clock_format_time(buffer + pos, buffer_size - pos,
                           end_hours, end_minutes, false);
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
  int avg_len = snprintf(buffer, buffer_size, "%s\n", i18n_get("30 DAY AVG", i18n_owner));
#else
  int avg_len = snprintf(buffer, buffer_size, "%s ", i18n_get("30 DAY", i18n_owner));
#endif

  if (daily_avg <= 0) {
    strncpy(buffer + avg_len, EN_DASH, buffer_size - avg_len);
  } else {
    health_util_format_hours_and_minutes(buffer + avg_len, buffer_size - avg_len,
                                         daily_avg, i18n_owner);
  }
}

Window *health_sleep_detail_card_create(HealthData *health_data) {
  HealthSleepDetailCardData *card_data = app_zalloc_check(sizeof(HealthSleepDetailCardData));

  card_data->daily_avg = health_data_sleep_get_monthly_average(health_data);

  const GColor fill_color = PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorDarkGray);
  const GColor today_fill_color = PBL_IF_COLOR_ELSE(GColorElectricBlue, GColorDarkGray);

  health_detail_card_set_render_day_zones(card_data->zones,
                                          &card_data->num_zones,
                                          &card_data->weekly_max,
                                          true /* format hours and minutes */,
                                          false /* show crown */,
                                          fill_color,
                                          today_fill_color,
                                          health_data_sleep_get(health_data),
                                          card_data);

  const size_t buffer_len = 32;

  HealthDetailHeading *heading = &card_data->headings[card_data->num_headings++];

  *heading = (HealthDetailHeading) {
    .primary_label = (char *)i18n_get("SLEEP SESSION", card_data),
    .primary_value = app_zalloc_check(buffer_len),
    .fill_color = GColorWhite,
    .outline_color = PBL_IF_COLOR_ELSE(GColorClear, GColorBlack),
  };

  prv_set_sleep_session(heading->primary_value, buffer_len,
                        health_data_sleep_get_start_time(health_data),
                        health_data_sleep_get_end_time(health_data));

  heading = &card_data->headings[card_data->num_headings++];

  *heading = (HealthDetailHeading) {
    .primary_label = (char *)i18n_get("DEEP SLEEP", card_data),
    .primary_value = app_zalloc_check(buffer_len),
    .fill_color = PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorWhite),
#if PBL_BW
    .outline_color = GColorBlack,
#endif
  };

  prv_set_deep_sleep(heading->primary_value, buffer_len,
                     health_data_current_deep_sleep_get(health_data), card_data);

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
    .bg_color = PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorWhite),
    .num_zones = card_data->num_zones,
    .zones = card_data->zones,
    .data = card_data,
  };

  return (Window *)health_detail_card_create(&config);
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
  i18n_free_all(card_data);
  app_free(card_data);
  health_detail_card_destroy(card);
}
