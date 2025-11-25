/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "health_data.h"
#include "health_progress.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/ui/content_indicator_private.h"
#include "applib/ui/ui.h"

#define MAX_NUM_HEADINGS (2)
#define MAX_NUM_SUBTITLES (2)
#define MAX_NUM_ZONES (7)

typedef struct HealthDetailHeading {
  char *primary_label;
  char *primary_value;
  char *secondary_label;
  char *secondary_value;
  GColor fill_color;
  GColor outline_color;
} HealthDetailHeading;

typedef struct HealthDetailSubtitle {
  char *label;
  GColor fill_color;
  GColor outline_color;
} HealthDetailSubtitle;

typedef struct HealthDetailZone {
  char *label;
  bool show_crown;
  bool hide_typical;
  GColor fill_color;
  HealthProgressBarValue progress;
} HealthDetailZone;

typedef struct HealthDetailCardConfig {
  GColor bg_color;
  int16_t num_headings;
  HealthDetailHeading *headings;
  int16_t num_subtitles;
  HealthDetailSubtitle *subtitles;
  int32_t daily_avg;
  int32_t weekly_max;
  int16_t num_zones;
  HealthDetailZone *zones;
  void *data;
} HealthDetailCardConfig;

typedef struct HealthDetailCard {
  Window window;
#if PBL_ROUND
  MenuLayer menu_layer;
  Layer down_arrow_layer;
  Layer up_arrow_layer;
  ContentIndicator down_indicator;
  ContentIndicator up_indicator;
#else
  ScrollLayer scroll_layer;
#endif

  GColor bg_color;

  int16_t num_headings;
  HealthDetailHeading *headings;

  int16_t num_subtitles;
  HealthDetailSubtitle *subtitles;

  GFont heading_label_font;
  GFont heading_value_font;
  GFont subtitle_font;

  GDrawCommandImage *icon_crown;

  int32_t daily_avg;
  int32_t max_progress;

  int16_t num_zones;
  HealthDetailZone *zones;

  int16_t y_origin;

  void *data;
} HealthDetailCard;

//! Creates a HealthDetailCard
HealthDetailCard *health_detail_card_create(const HealthDetailCardConfig *config);

//! Destroys a HealthDetailCard
void health_detail_card_destroy(HealthDetailCard *detail_card);

//! Configures a HealthDetailCard
void health_detail_card_configure(HealthDetailCard *detail_card,
                                  const HealthDetailCardConfig *config);

//! Sets the zones for any daily history (steps/sleep)
//! @param zones pointer to the HealthDetailZone to be set
//! @param num_zones number of zones in the pointer
//! @wparam weekly_max pointer to the weekly max to be set
//! @param format_hours_and_minutes whether to a format the values for hours and minutes in label
//! @param show_crown whether to set the `show_crown` in the zone with weekly max
//! @param fill_color color to fill all the progress bars with except today
//! @param today_fill_color color to fill the today progress bar with
//! @param day_data pointer to the daily history data
//! @param i18n_owner pointer to the i18n owner
void health_detail_card_set_render_day_zones(HealthDetailZone *zones, int16_t *num_zones,
    int32_t *weekly_max, bool format_hours_and_minutes, bool show_crown, GColor fill_color,
    GColor today_fill_color, int32_t *day_data, void *i18n_owner);
