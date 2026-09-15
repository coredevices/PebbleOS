/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "pbl/services/activity/activity.h"

static inline int workout_header_height(void) {
  return PBL_IF_ROUND_ELSE(48, 40);
}

static inline GColor workout_activity_color(ActivitySessionType type) {
  return PBL_IF_COLOR_ELSE(type == ActivitySessionType_Run    ? GColorChromeYellow
                           : type == ActivitySessionType_Walk ? GColorPictonBlue
                                                              : GColorScreaminGreen,
                           GColorWhite);
}

static inline void workout_draw_select_indicator(GContext *ctx, GSize size) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_oval(ctx, GRect(size.w - 5, (size.h - 26) / 2, 26, 26), GOvalScaleModeFitCircle);
}
