/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/activity/activity.h>
#include <pbl/util/size.h>

#include <applib/graphics/gpath.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/gtypes.h>

static inline GColor workout_activity_color(ActivitySessionType type) {
  return PBL_IF_COLOR_ELSE(type == ActivitySessionType_Run    ? GColorChromeYellow
                           : type == ActivitySessionType_Walk ? GColorPictonBlue
                                                              : GColorScreaminGreen,
                           GColorWhite);
}

static inline GColor workout_activity_text_color(ActivitySessionType type) {
  return PBL_IF_COLOR_ELSE(type == ActivitySessionType_Run    ? GColorWindsorTan
                           : type == ActivitySessionType_Walk ? GColorCobaltBlue
                                                              : GColorIslamicGreen,
                           GColorBlack);
}

// Same outline as the heart in the Health app.
static inline void workout_fill_heart(GContext *ctx, GRect box, GColor color) {
  static const GPoint shape[] = {
    {0, 16}, {0, 8}, {8, 0}, {14, 6}, {15, 6}, {21, 0}, {29, 8}, {29, 16}, {15, 30}, {14, 30},
  };
  GPoint points[ARRAY_LENGTH(shape)];
  for (size_t i = 0; i < ARRAY_LENGTH(shape); i++) {
    points[i] = GPoint(box.origin.x + shape[i].x * (box.size.w - 1) / 29,
                       box.origin.y + shape[i].y * (box.size.h - 1) / 30);
  }
  GPath path = {.num_points = ARRAY_LENGTH(points), .points = points};
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_antialiased(ctx, true);
  gpath_draw_filled(ctx, &path);
}

static inline int workout_heading_top(void) {
  return PBL_IF_ROUND_ELSE(16, 6);
}

// Accent rule under the activity heading of the workout screens.
static inline GRect workout_heading_rule(GSize size) {
  return GRect(size.w / 2 - 14, workout_heading_top() + 25, 28, 3);
}

static inline void workout_draw_select_indicator(GContext *ctx, GSize size) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_oval(ctx, GRect(size.w - 5, (size.h - 26) / 2, 26, 26), GOvalScaleModeFitCircle);
}
