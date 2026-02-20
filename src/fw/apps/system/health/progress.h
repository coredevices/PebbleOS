/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/ui.h"

#define HEALTH_PROGRESS_BAR_MAX_VALUE 0xfff

typedef int32_t HealthProgressBarValue;

typedef enum HealthProgressSegmentType {
  HealthProgressSegmentType_Horizontal,
  HealthProgressSegmentType_Vertical,
  HealthProgressSegmentType_Corner,
  HealthProgressSegmentTypeCount,
} HealthProgressSegmentType;

typedef struct HealthProgressSegment {
  HealthProgressSegmentType type;
  // The amount of the total progress bar that this segment occupies.
  // Summing this value over all segments should total HEALTH_PROGRESS_BAR_MAX_VALUE
  int amount_of_total;
  int mark_width;
  GPoint points[4];
} HealthProgressSegment;

typedef struct HealthProgressBar {
  int num_segments;
  HealthProgressSegment *segments;
} HealthProgressBar;


void health_progress_bar_fill(GContext *ctx, HealthProgressBar *progress_bar, GColor color,
                              HealthProgressBarValue start, HealthProgressBarValue end);

void health_progress_bar_mark(GContext *ctx, HealthProgressBar *progress_bar, GColor color,
                              HealthProgressBarValue value_to_mark);

void health_progress_bar_outline(GContext *ctx, HealthProgressBar *progress_bar, GColor color);
