/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "health_progress.h"

#include "applib/graphics/gpath_builder.h"

#include "system/logging.h"

// Scales a total shape offset to an individual segment offset.
// @param total_offset should not be larger than the segment's percent of total
static int prv_total_offset_to_segment_offset(HealthProgressSegment *segment,
                                              int total_offset) {
  return total_offset * HEALTH_PROGRESS_BAR_MAX_VALUE / segment->amount_of_total;
}

static bool prv_is_segment_corner(HealthProgressSegment *segment) {
  return (segment->type == HealthProgressSegmentType_Corner);
}

static GPointPrecise prv_get_adjusted_gpoint_precise_from_gpoint(GPoint point) {
  GPointPrecise pointP = GPointPreciseFromGPoint(point);
  // Hack to make it draw 2px lines on b/w
  // Note that this shifts it down and to the right, but it works for us.
  pointP.x.fraction += FIXED_S16_3_HALF.raw_value;
  pointP.y.fraction += FIXED_S16_3_HALF.raw_value;
  return pointP;
}

static GPoint prv_get_point_between_points(GPoint p1, GPoint p2, HealthProgressBarValue val) {
  int x = p1.x + ((p2.x - p1.x) * val / HEALTH_PROGRESS_BAR_MAX_VALUE);
  int y = p1.y + ((p2.y - p1.y) * val / HEALTH_PROGRESS_BAR_MAX_VALUE);

  return GPoint(x, y);
}

static void prv_fill_segment(GContext *ctx, HealthProgressSegment *segment, GColor color,
                             HealthProgressBarValue start, HealthProgressBarValue end) {
  GPoint p1, p2, p3, p4;
  if (segment->type == HealthProgressSegmentType_Vertical) {
    p1 = prv_get_point_between_points(segment->points[0], segment->points[3], start);
    p2 = prv_get_point_between_points(segment->points[1], segment->points[2], start);
    p3 = prv_get_point_between_points(segment->points[1], segment->points[2], end);
    p4 = prv_get_point_between_points(segment->points[0], segment->points[3], end);
  } else if (segment->type == HealthProgressSegmentType_Horizontal) {
    p1 = prv_get_point_between_points(segment->points[0], segment->points[1], start);
    p2 = prv_get_point_between_points(segment->points[3], segment->points[2], start);
    p3 = prv_get_point_between_points(segment->points[3], segment->points[2], end);
    p4 = prv_get_point_between_points(segment->points[0], segment->points[1], end);
  } else {
    p1 = segment->points[0];
    p2 = segment->points[1];
    p3 = segment->points[2];
    p4 = segment->points[3];
  }

  GPathBuilder *builder = gpath_builder_create(5);
  gpath_builder_move_to_point(builder, p1);
  gpath_builder_line_to_point(builder, p2);
  gpath_builder_line_to_point(builder, p3);
  gpath_builder_line_to_point(builder, p4);
  GPath *path = gpath_builder_create_path(builder);
  gpath_builder_destroy(builder);

  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

void health_progress_bar_fill(GContext *ctx, HealthProgressBar *progress_bar, GColor color,
                              HealthProgressBarValue start, HealthProgressBarValue end) {
  if (start < 0) {
    // This ensures we don't deal with negative values
    start += HEALTH_PROGRESS_BAR_MAX_VALUE;
  }
  if (start > end) {
    // This ensures the end is always after the start
    end += HEALTH_PROGRESS_BAR_MAX_VALUE;
  }

  int amount_traversed = 0;
  HealthProgressSegment *segment = progress_bar->segments;
  while (start >= amount_traversed + segment->amount_of_total) {
    // Skip until the segment which includes the start
    amount_traversed += segment->amount_of_total;
    segment++;
  }

  if (prv_is_segment_corner(segment)) {
    segment++;
  }

  while (amount_traversed < end) {
    if (prv_is_segment_corner(segment)) {
      // Fully fill corner segments for now
      prv_fill_segment(ctx, segment, color, 0, HEALTH_PROGRESS_BAR_MAX_VALUE);
      segment++;
      continue;
    }

    const int from_total = MAX(start, amount_traversed) - amount_traversed;
    const int to_total = MIN(end, amount_traversed + segment->amount_of_total) - amount_traversed;

    const int from = prv_total_offset_to_segment_offset(segment, from_total);
    const int to = prv_total_offset_to_segment_offset(segment, to_total);

    prv_fill_segment(ctx, segment, color, from, to);

    amount_traversed += segment->amount_of_total;

    if (segment == &progress_bar->segments[progress_bar->num_segments - 1]) {
      // We are on the last segment, wrap back to the first
      segment = &progress_bar->segments[0];
    } else {
      segment++;
    }
  }
}

void health_progress_bar_mark(GContext *ctx, HealthProgressBar *progress_bar, GColor color,
                              HealthProgressBarValue value_to_mark) {
  if (value_to_mark < 0) {
    // This ensures we don't deal with negative values
    value_to_mark += HEALTH_PROGRESS_BAR_MAX_VALUE;
  }

  HealthProgressSegment *segment = progress_bar->segments;
  while (value_to_mark > segment->amount_of_total) {
    value_to_mark -= segment->amount_of_total;
    segment++;
  }

  if (prv_is_segment_corner(segment)) {
    segment++;
  }

  const int from = prv_total_offset_to_segment_offset(segment, value_to_mark);

  // Fill backwards if we can, otherwise forwards
  const int dir = value_to_mark - segment->mark_width < 0 ? 1 : -1;
  const int to_total = value_to_mark + (dir * segment->mark_width);
  const int to = prv_total_offset_to_segment_offset(segment, to_total);

  prv_fill_segment(ctx, segment, color, from, to);
}

void health_progress_bar_outline(GContext *ctx, HealthProgressBar *progress_bar, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);

  for (int i = 0; i < progress_bar->num_segments; i++) {
    HealthProgressSegment *segment = &progress_bar->segments[i];

    GPointPrecise p0 = prv_get_adjusted_gpoint_precise_from_gpoint(segment->points[0]);
    GPointPrecise p1 = prv_get_adjusted_gpoint_precise_from_gpoint(segment->points[1]);
    GPointPrecise p2 = prv_get_adjusted_gpoint_precise_from_gpoint(segment->points[2]);
    GPointPrecise p3 = prv_get_adjusted_gpoint_precise_from_gpoint(segment->points[3]);

    if (segment->type == HealthProgressSegmentType_Vertical) {
      graphics_line_draw_precise_stroked(ctx, p0, p3);
      graphics_line_draw_precise_stroked(ctx, p1, p2);
    } else if (segment->type == HealthProgressSegmentType_Horizontal) {
      graphics_line_draw_precise_stroked(ctx, p0, p1);
      graphics_line_draw_precise_stroked(ctx, p2, p3);
    } else {
      graphics_line_draw_precise_stroked(ctx, p1, p2);
      graphics_line_draw_precise_stroked(ctx, p2, p3);
    }
  }
}
