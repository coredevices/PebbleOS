/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/util/units.h>

#include <applib/graphics/gtypes.h>

//! Background the chart is drawn over
#define HEALTH_HR_CHART_BG_COLOR PBL_IF_COLOR_ELSE(GColorMelon, GColorWhite)

//! Readings further apart than this (in BPM) split a bucket into separate clusters
#define HEALTH_HR_CHART_CLUSTER_GAP_BPM 10

//! A group of nearby readings within a time bucket; a single value when min equals max
typedef struct {
  uint8_t min_bpm;
  uint8_t max_bpm;
} HealthHrChartCluster;

typedef struct {
  //! Heart rate for each minute since local midnight, 0 when there is no reading
  const uint8_t *minute_bpm;
  //! Number of entries in minute_bpm
  uint16_t num_minutes;
} HealthHrChartData;

//! Sorts readings in place and groups them into clusters split at gaps larger than
//! HEALTH_HR_CHART_CLUSTER_GAP_BPM.
//! @return Number of clusters written, at most max_clusters
size_t health_hr_chart_cluster(uint8_t *bpm, size_t num_bpm, HealthHrChartCluster *clusters,
                               size_t max_clusters);

//! A day's readings grouped for drawing, built once by health_hr_chart_init
typedef struct {
  //! Every hour's clusters in hour order, null when there are no readings
  HealthHrChartCluster *clusters;
  //! Index in clusters of each hour's first cluster, then the total number of clusters
  uint16_t hour_start[PBL_HOUR_PER_DAY + 1];
  //! Minute of the latest reading, -1 if there are none
  int16_t latest_minute;
  uint8_t latest_bpm;
  uint8_t min_bpm;
  uint8_t max_bpm;
  //! Whether hour labels have room for AM/PM, worked out on the first draw
  bool am_pm_checked;
  bool show_am_pm;
} HealthHrChart;

//! Groups the day's readings into hourly clusters. The data isn't needed after this returns.
void health_hr_chart_init(HealthHrChart *chart, const HealthHrChartData *data);

//! Frees what health_hr_chart_init allocated
void health_hr_chart_deinit(HealthHrChart *chart);

//! Draws a 24 hour range chart of the day's heart rate, one bar per hour, in the given bounds
void health_hr_chart_draw(GContext *ctx, const GRect *bounds, HealthHrChart *chart);
