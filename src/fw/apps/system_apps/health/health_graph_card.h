/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "health_data.h"

#include "applib/fonts/fonts.h"
#include "applib/ui/layer.h"
#include "util/time/time.h"

typedef enum {
  HealthGraphIndex_Sunday = Sunday,
  HealthGraphIndex_Monday = Monday,
  HealthGraphIndex_Saturday = Saturday,
  HealthGraphIndex_Average = HealthGraphIndex_Sunday + DAYS_PER_WEEK,
  HealthGraphIndexCount,
} HealthGraphIndex;

typedef struct HealthGraphCard HealthGraphCard;

typedef void (*HealthGraphCardInfoUpdate)(HealthGraphCard *graph_card, int32_t day_point,
                                          char *buffer, size_t buffer_size);

typedef struct {
  WeeklyStats stats;
  time_t timestamp;
  int32_t *day_data;
  int32_t default_max;
} HealthGraphCardData;

typedef struct {
  const char *title;
  const char *info_avg;
  const HealthGraphCardData *graph_data;
  HealthGraphCardInfoUpdate info_update;
  size_t info_buffer_size;
  const GColor inactive_color;
} HealthGraphCardConfig;

struct HealthGraphCard {
  Layer layer;

  WeeklyStats stats;
  //! Today is 0. Save up to and including last week's day of the same week day
  int32_t day_data[DAYS_PER_WEEK + 1];
  time_t data_timestamp; //!< Time at which the data applies in UTC seconds
  int32_t data_max;

  GFont title_font;
  GFont legend_font;
  const char *day_chars;
  const char *title;
  const char *info_avg;
  GColor inactive_color;

  HealthGraphCardInfoUpdate info_update;
  size_t info_buffer_size;

  uint8_t current_day; //!< Current weekday (weekend inclusive) where Sunday is first at 0
  HealthGraphIndex selection;
};

//! Creates a HealthGraphCard
HealthGraphCard *health_graph_card_create(const HealthGraphCardConfig *config);

//! Destroys a HealthGraphCard
void health_graph_card_destroy(HealthGraphCard *graph_card);

//! Configures a HealthGraphCard
void health_graph_card_configure(HealthGraphCard *graph_card, const HealthGraphCardConfig *config);

//! Cycles the HealthGraphCard selection
void health_graph_card_cycle_selected(HealthGraphCard *graph_card);

//! Formats a string with a prefix of the current weekday selection
size_t health_graph_format_weekday_prefix(HealthGraphCard *graph_card, char *buffer,
                                          size_t buffer_size);
