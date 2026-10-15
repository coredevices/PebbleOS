/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hr_chart.h"

#include <stdio.h>
#include <string.h>

#include <pbl/services/clock.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/services/time.h>
#include <pbl/util/math.h>
#include <pbl/util/string.h>
#include <pbl/util/units.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/graphics_circle.h>
#include <applib/graphics/text.h>
#include <kernel/pbl_malloc.h>

#define BAR_COLOR         PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorBlack)
#define MARKER_COLOR      BAR_COLOR
#define MARKER_FILL_COLOR GColorWhite
#define TEXT_COLOR        gcolor_legible_over(HEALTH_HR_CHART_BG_COLOR)
#define GRID_COLOR        PBL_IF_COLOR_ELSE(GColorRoseVale, GColorBlack)

//! Smallest BPM span of the y axis, so flat days don't exaggerate small changes
#define MIN_BPM_SPAN 30
//! The y axis ends on multiples of this, so its labels read as a scale
#define BPM_AXIS_STEP 10
#define MAX_BAR_WIDTH 4
#define GRID_HOURS    6
#define LABEL_PADDING 3
//! Empty rows between the axis lines and the bars
#define AXIS_GAP 1
//! Empty columns between the end of the axis lines and the widest label
#define AXIS_LINE_PADDING 5
#define BUCKET_MINUTES    PBL_MIN_PER_HOUR
#define NUM_BUCKETS       PBL_HOUR_PER_DAY
//! Empty columns between a grid line and the bars on either side of it
#define GRID_GAP       1
#define GRID_BUCKETS   (GRID_HOURS * PBL_MIN_PER_HOUR / BUCKET_MINUTES)
#define NUM_GRID_CELLS (NUM_BUCKETS / GRID_BUCKETS)

size_t health_hr_chart_cluster(uint8_t *bpm, size_t num_bpm, HealthHrChartCluster *clusters,
                               size_t max_clusters) {
  if (num_bpm == 0 || max_clusters == 0) {
    return 0;
  }

  // Insertion sort, buckets hold at most an hour of readings
  for (size_t i = 1; i < num_bpm; i++) {
    const uint8_t value = bpm[i];
    size_t j = i;
    for (; j > 0 && bpm[j - 1] > value; j--) {
      bpm[j] = bpm[j - 1];
    }
    bpm[j] = value;
  }

  size_t num_clusters = 0;
  clusters[0] = (HealthHrChartCluster){.min_bpm = bpm[0], .max_bpm = bpm[0]};
  for (size_t i = 1; i < num_bpm; i++) {
    if (bpm[i] - bpm[i - 1] > HEALTH_HR_CHART_CLUSTER_GAP_BPM) {
      if (++num_clusters == max_clusters) {
        return num_clusters;
      }
      clusters[num_clusters].min_bpm = bpm[i];
    }
    clusters[num_clusters].max_bpm = bpm[i];
  }
  return num_clusters + 1;
}

//! @return Minute of the latest reading, -1 if there are none
static int prv_get_range(const HealthHrChartData *data, uint8_t *min_bpm, uint8_t *max_bpm) {
  *min_bpm = UINT8_MAX;
  *max_bpm = 0;
  int latest_minute = -1;
  for (int i = 0; i < MIN(data->num_minutes, PBL_MIN_PER_DAY); i++) {
    const uint8_t bpm = data->minute_bpm[i];
    if (bpm) {
      *min_bpm = MIN(*min_bpm, bpm);
      *max_bpm = MAX(*max_bpm, bpm);
      latest_minute = i;
    }
  }
  return latest_minute;
}

//! Clusters one bucket's readings into clusters, which has room for BUCKET_MINUTES
static size_t prv_cluster_bucket(const HealthHrChartData *data, int bucket,
                                 HealthHrChartCluster *clusters) {
  uint8_t readings[BUCKET_MINUTES];
  size_t num_readings = 0;
  const int start = bucket * BUCKET_MINUTES;
  const int end = MIN(start + BUCKET_MINUTES, MIN(data->num_minutes, PBL_MIN_PER_DAY));
  for (int i = start; i < end; i++) {
    if (data->minute_bpm[i]) {
      readings[num_readings++] = data->minute_bpm[i];
    }
  }
  return health_hr_chart_cluster(readings, num_readings, clusters, BUCKET_MINUTES);
}

void health_hr_chart_init(HealthHrChart *chart, const HealthHrChartData *data) {
  *chart = (HealthHrChart){};
  chart->latest_minute = prv_get_range(data, &chart->min_bpm, &chart->max_bpm);
  if (chart->latest_minute < 0) {
    return;
  }
  chart->latest_bpm = data->minute_bpm[chart->latest_minute];

  // Count the clusters first so they fit in a single allocation
  HealthHrChartCluster clusters[BUCKET_MINUTES];
  uint16_t num_clusters = 0;
  for (int bucket = 0; bucket < NUM_BUCKETS; bucket++) {
    chart->hour_start[bucket] = num_clusters;
    num_clusters += prv_cluster_bucket(data, bucket, clusters);
  }
  chart->hour_start[NUM_BUCKETS] = num_clusters;

  chart->clusters = app_malloc_check(num_clusters * sizeof(*chart->clusters));
  for (int bucket = 0; bucket < NUM_BUCKETS; bucket++) {
    prv_cluster_bucket(data, bucket, &chart->clusters[chart->hour_start[bucket]]);
  }
}

void health_hr_chart_deinit(HealthHrChart *chart) {
  app_free(chart->clusters);
  chart->clusters = nullptr;
}

typedef struct {
  GRect plot;
  int16_t pitch;
  int16_t bar_width;
  int16_t grid_spacing;
  int16_t lo_bpm;
  int16_t hi_bpm;
} ChartLayout;

//! Solid on color displays, dotted on black and white so lines stay lighter than the bars
static void prv_draw_grid_line(GContext *ctx, GPoint start, GPoint end) {
#if PBL_BW
  if (start.x == end.x) {
    graphics_draw_vertical_line_dotted(ctx, start, end.y - start.y + 1);
  } else {
    graphics_draw_horizontal_line_dotted(ctx, start, end.x - start.x + 1);
  }
#else
  graphics_draw_line(ctx, start, end);
#endif
}

static int16_t prv_bar_width(int16_t pitch) {
  return CLIP(pitch - 1, 1, MAX_BAR_WIDTH);
}

//! Width of the cell between two grid lines, sized so its bars clear both lines by GRID_GAP
static int16_t prv_grid_spacing(int16_t pitch) {
  return ((GRID_BUCKETS - 1) * pitch) + prv_bar_width(pitch) + 1 + (2 * GRID_GAP);
}

static int16_t prv_bucket_x(const ChartLayout *layout, int bucket) {
  return layout->plot.origin.x + ((bucket / GRID_BUCKETS) * layout->grid_spacing) + 1 + GRID_GAP +
         ((bucket % GRID_BUCKETS) * layout->pitch);
}

static int16_t prv_bpm_to_y(const ChartLayout *layout, uint8_t bpm) {
  // Keep room at both ends for half a bar and the axis line beyond it
  const int16_t inset = (layout->bar_width / 2) + 1 + AXIS_GAP;
  const int16_t span_px = layout->plot.size.h - 1 - (2 * inset);
  const int16_t bottom = layout->plot.origin.y + layout->plot.size.h - 1 - inset;
  return bottom - ((bpm - layout->lo_bpm) * span_px) / (layout->hi_bpm - layout->lo_bpm);
}

//! Top row of a bar whose highest reading is bpm
static int16_t prv_bar_top(const ChartLayout *layout, uint8_t bpm) {
  return prv_bpm_to_y(layout, bpm) - (layout->bar_width / 2);
}

//! Row just below a bar whose lowest reading is bpm
static int16_t prv_bar_bottom(const ChartLayout *layout, uint8_t bpm) {
  return prv_bar_top(layout, bpm) + layout->bar_width;
}

static void prv_draw_cluster(GContext *ctx, const ChartLayout *layout, int16_t x,
                             const HealthHrChartCluster *cluster) {
  const int16_t top = prv_bar_top(layout, cluster->max_bpm);
  const GRect rect =
      GRect(x, top, layout->bar_width, prv_bar_bottom(layout, cluster->min_bpm) - top);
  graphics_fill_round_rect(ctx, &rect, layout->bar_width / 2, GCornersAll);
}

static void prv_format_hour(char *buffer, size_t size, int hour, bool show_am_pm) {
  const char *format =
      clock_is_24h_style() ? i18n_noop("%k") : (show_am_pm ? i18n_noop("%l%p") : i18n_noop("%l"));
  const struct pbl_tm time_tm = {.tm_hour = hour};
  pbl_strftime_r(buffer, size, i18n_get(format, buffer), &time_tm);
  i18n_free(format, buffer);
  // %k and %l pad single digits with a space
  const char *start = string_strip_leading_whitespace(buffer);
  memmove(buffer, start, strlen(start) + 1);
}

static int16_t prv_text_width(GContext *ctx, const char *text, GFont font) {
  return graphics_text_layout_get_max_used_size(ctx, text, font, GRect(0, 0, INT16_MAX, INT16_MAX),
                                                GTextOverflowModeFill, GTextAlignmentLeft, nullptr)
      .w;
}

//! Whether the widest hour label still fits with its AM/PM designator
static bool prv_am_pm_fits(GContext *ctx, GFont font, int16_t max_width) {
  for (int hour = 0; hour < PBL_HOUR_PER_DAY; hour += GRID_HOURS) {
    char label[16];
    prv_format_hour(label, sizeof(label), hour, true);
    if (prv_text_width(ctx, label, font) > max_width) {
      return false;
    }
  }
  return true;
}

static void prv_draw_grid(GContext *ctx, const ChartLayout *layout, GFont font,
                          HealthHrChart *chart) {
  const GRect *plot = &layout->plot;
  const int16_t label_y = plot->origin.y + plot->size.h;
  const int16_t label_h = fonts_get_font_height(font) + LABEL_PADDING;
  const int16_t grid_spacing = layout->grid_spacing;
  const int16_t label_w = grid_spacing - LABEL_PADDING;
  if (!chart->am_pm_checked) {
    chart->show_am_pm = prv_am_pm_fits(ctx, font, label_w);
    chart->am_pm_checked = true;
  }
  graphics_context_set_stroke_color(ctx, GRID_COLOR);
  graphics_context_set_text_color(ctx, TEXT_COLOR);
  for (int hour = 0; hour <= PBL_HOUR_PER_DAY; hour += GRID_HOURS) {
    const int16_t x = plot->origin.x + (hour / GRID_HOURS) * grid_spacing;
    prv_draw_grid_line(ctx, GPoint(x, plot->origin.y), GPoint(x, label_y + label_h - 1));
    if (hour == PBL_HOUR_PER_DAY) {
      break;
    }

    char label[16];
    prv_format_hour(label, sizeof(label), hour, chart->show_am_pm);
    const GRect label_rect = GRect(x + 2, label_y - 1, label_w, label_h);
    graphics_draw_text(ctx, label, font, label_rect, GTextOverflowModeFill, GTextAlignmentLeft,
                       nullptr);
  }
}

//! Range label centred on the row y, at the right of the column
static GRect prv_range_label_rect(const GRect *column, int16_t y, GFont font) {
  const int16_t font_h = fonts_get_font_height(font);
  // Gothic glyphs sit low in their box, so pull the text up to centre it on the row
  const int16_t text_offset = (font_h * 2) / 3;
  return GRect(column->origin.x, y - text_offset, column->size.w, font_h + LABEL_PADDING);
}

//! Line across the chart at row y, drawn under the bars
static void prv_draw_axis_line(GContext *ctx, const ChartLayout *layout, int16_t y, int16_t end_x) {
  graphics_context_set_stroke_color(ctx, GRID_COLOR);
  prv_draw_grid_line(ctx, GPoint(layout->plot.origin.x, y), GPoint(end_x - 1, y));
}

static void prv_draw_range_label(GContext *ctx, const GRect *column, int16_t y, const char *text,
                                 GFont font) {
  const GRect rect = prv_range_label_rect(column, y, font);
  graphics_context_set_text_color(ctx, TEXT_COLOR);
  graphics_draw_text(ctx, text, font, rect, GTextOverflowModeFill, GTextAlignmentRight, nullptr);
}

static void prv_draw_latest(GContext *ctx, const ChartLayout *layout, int16_t x, uint8_t bpm) {
  // Same box as a single reading's bar, so the marker is centred on it even at even widths
  const GRect bar = GRect(x, prv_bar_top(layout, bpm), layout->bar_width, layout->bar_width);
  graphics_context_set_fill_color(ctx, HEALTH_HR_CHART_BG_COLOR);
  graphics_fill_oval(ctx, grect_inset(bar, GEdgeInsets(-3)), GOvalScaleModeFitCircle);
  graphics_context_set_fill_color(ctx, MARKER_COLOR);
  graphics_fill_oval(ctx, grect_inset(bar, GEdgeInsets(-2)), GOvalScaleModeFitCircle);
  graphics_context_set_fill_color(ctx, MARKER_FILL_COLOR);
  graphics_fill_oval(ctx, bar, GOvalScaleModeFitCircle);
}

//! Rounds the range out to whole axis steps of at least MIN_BPM_SPAN, growing whichever end
//! leaves the readings nearer the edge
static void prv_get_axis(uint8_t min_bpm, uint8_t max_bpm, int16_t *lo_bpm, int16_t *hi_bpm) {
  int16_t lo = (min_bpm / BPM_AXIS_STEP) * BPM_AXIS_STEP;
  int16_t hi = ((max_bpm + BPM_AXIS_STEP - 1) / BPM_AXIS_STEP) * BPM_AXIS_STEP;
  while (hi - lo < MIN_BPM_SPAN) {
    if (lo > 0 && (min_bpm - lo) <= (hi - max_bpm)) {
      lo -= BPM_AXIS_STEP;
    } else {
      hi += BPM_AXIS_STEP;
    }
  }
  *lo_bpm = lo;
  *hi_bpm = hi;
}

void health_hr_chart_draw(GContext *ctx, const GRect *bounds, HealthHrChart *chart) {
  const uint8_t min_bpm = chart->min_bpm;
  const uint8_t max_bpm = chart->max_bpm;

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  const int16_t font_h = fonts_get_font_height(font);
  const int16_t label_column_w = prv_text_width(ctx, "000", font) + LABEL_PADDING;

  GRect plot = *bounds;
  plot.size.w -= label_column_w;
  plot.size.h -= font_h + LABEL_PADDING;

  int16_t pitch = MAX(plot.size.w / NUM_BUCKETS, 1);
  while (pitch > 1 && (NUM_GRID_CELLS * prv_grid_spacing(pitch)) > plot.size.w) {
    pitch--;
  }
  plot.size.w = NUM_GRID_CELLS * prv_grid_spacing(pitch);
  ChartLayout layout = {
    .plot = plot,
    .pitch = pitch,
    .bar_width = prv_bar_width(pitch),
    .grid_spacing = prv_grid_spacing(pitch),
  };
  prv_draw_grid(ctx, &layout, font, chart);
  if (chart->latest_minute < 0) {
    return;
  }

  // Bars are pixel aligned; the width left over goes to the label column, so the chart spans
  // the full bounds
  const GRect label_column =
      GRect(plot.origin.x + plot.size.w, plot.origin.y, bounds->size.w - plot.size.w, plot.size.h);
  prv_get_axis(min_bpm, max_bpm, &layout.lo_bpm, &layout.hi_bpm);

  // The axis lines frame the plot, and stop short of the wider label
  const int16_t max_y = plot.origin.y;
  const int16_t min_y = plot.origin.y + plot.size.h - 1;
  char max_text[8];
  char min_text[8];
  snprintf(max_text, sizeof(max_text), "%d", layout.hi_bpm);
  snprintf(min_text, sizeof(min_text), "%d", layout.lo_bpm);
  const int16_t text_w =
      MAX(prv_text_width(ctx, max_text, font), prv_text_width(ctx, min_text, font));
  const int16_t line_end_x =
      label_column.origin.x + label_column.size.w - text_w - AXIS_LINE_PADDING;
  prv_draw_axis_line(ctx, &layout, max_y, line_end_x);
  prv_draw_axis_line(ctx, &layout, min_y, line_end_x);

  graphics_context_set_fill_color(ctx, BAR_COLOR);
  for (int bucket = 0; bucket < NUM_BUCKETS; bucket++) {
    const int16_t x = prv_bucket_x(&layout, bucket);
    for (int i = chart->hour_start[bucket]; i < chart->hour_start[bucket + 1]; i++) {
      prv_draw_cluster(ctx, &layout, x, &chart->clusters[i]);
    }
  }

  prv_draw_range_label(ctx, &label_column, max_y, max_text, font);
  prv_draw_range_label(ctx, &label_column, min_y, min_text, font);
  prv_draw_latest(ctx, &layout, prv_bucket_x(&layout, chart->latest_minute / BUCKET_MINUTES),
                  chart->latest_bpm);
}
