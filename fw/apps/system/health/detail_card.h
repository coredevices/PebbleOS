/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "data.h"
#include "progress.h"

#include <applib/event_service_client.h>
#include <applib/fonts/fonts.h>
#include <applib/graphics/gdraw_command_image.h>
#include <applib/platform.h>
#include <applib/ui/content_indicator_private.h>
#include <applib/ui/ui.h>

#define MAX_NUM_HEADINGS  (2)
#define MAX_NUM_SUBTITLES (2)
#define MAX_NUM_ZONES     (7)

//! Tall rectangular displays (e.g. emery) get larger fonts, roomier rows and an optional hero
//! header
#if PBL_RECT && PBL_DISPLAY_HEIGHT > LEGACY_2X_DISP_ROWS
#define HEALTH_DETAIL_TALL_RECT 1
#endif

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

typedef void (*HealthDetailChartDrawCallback)(GContext *ctx, const GRect *bounds, void *context);

//! A chart boxed in above the first heading; on round displays it gets a row of its own, below it
typedef struct HealthDetailChart {
  int16_t height;
  GColor bg_color; //!< Fill behind the chart
  HealthDetailChartDrawCallback draw;
  void *context;
} HealthDetailChart;

typedef void (*HealthDetailHeaderDrawCallback)(GContext *ctx, const GRect *bounds, void *context);

//! A full-width section drawn above everything else on tall rectangular displays (a hero page);
//! the content below it scrolls in as a second page
typedef struct HealthDetailHeader {
  int16_t height;
  HealthDetailHeaderDrawCallback draw;
  void *context;
} HealthDetailHeader;

typedef struct HealthDetailCardConfig {
  GColor bg_color;
  //! Use the system theme's background instead of bg_color, following dark mode flips
  bool bg_follows_theme;
  int16_t num_headings;
  HealthDetailHeading *headings;
  int16_t num_subtitles;
  HealthDetailSubtitle *subtitles;
  int32_t daily_avg;
  int32_t weekly_max;
  int16_t num_zones;
  HealthDetailZone *zones;
  const HealthDetailChart *chart;   //!< Optional, NULL for none
  const HealthDetailHeader *header; //!< Optional, NULL for none (tall rectangular displays only)
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
  EventServiceInfo dark_mode_event_info; //!< Only subscribed if the background follows the theme

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

  HealthDetailChart chart;   //!< No chart when draw is NULL
  HealthDetailHeader header; //!< No header when draw is NULL

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
//! @param weekly_max pointer to the weekly max to be set
//! @param format_hours_and_minutes whether to a format the values for hours and minutes in label
//! @param show_crown whether to set the `show_crown` in the zone with weekly max
//! @param fill_color color to fill all the progress bars with except today
//! @param today_fill_color color to fill the today progress bar with
//! @param day_data pointer to the daily history data
//! @param i18n_owner pointer to the i18n owner
void health_detail_card_set_render_day_zones(HealthDetailZone *zones, int16_t *num_zones,
                                             int32_t *weekly_max, bool format_hours_and_minutes,
                                             bool show_crown, GColor fill_color,
                                             GColor today_fill_color, int32_t *day_data,
                                             void *i18n_owner);
