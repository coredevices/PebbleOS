/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/activity/activity.h>

#include <applib/graphics/graphics.h>
#include <applib/graphics/gtypes.h>
#include <applib/ui/animation.h>
#include <applib/ui/kino/kino_reel.h>

static inline GColor workout_activity_color(ActivitySessionType type) {
  return PBL_IF_COLOR_ELSE(type == ActivitySessionType_Run    ? GColorChromeYellow
                           : type == ActivitySessionType_Walk ? GColorPictonBlue
                                                              : GColorScreaminGreen,
                           GColorWhite);
}

// Deeper than the activity color so the white action bar icons stand out.
static inline GColor workout_action_bar_color(ActivitySessionType type) {
  return PBL_IF_COLOR_ELSE(type == ActivitySessionType_Run    ? GColorOrange
                           : type == ActivitySessionType_Walk ? GColorCobaltBlue
                                                              : GColorIslamicGreen,
                           GColorBlack);
}

// Size of the dot the activity icons shrink into and unfold from.
#define WORKOUT_ICON_DOT_RADIUS 5

// System activity icon as line art in the activity color, to read on the dark background.
KinoReel *workout_create_activity_icon(ActivitySessionType type);

// Same outline as the heart in the Health app.
void workout_fill_heart(GContext *ctx, GRect box, GColor color);

static inline int workout_status_height(GSize size) {
  return PBL_IF_ROUND_ELSE(42, size.h >= 200 ? 30 : 18);
}

// Accent rule under the status strip of the workout screens.
static inline GRect workout_heading_rule(GSize size) {
  const int inset = PBL_IF_ROUND_ELSE(24, 0);
  return GRect(inset, workout_status_height(size) - 2, size.w - 2 * inset, 2);
}

void workout_draw_select_indicator(GContext *ctx, GSize size);

// Animations auto destroy: unscheduling is enough.
static inline void workout_cancel_animation(Animation **animation) {
  if (*animation) {
    Animation *running = *animation;
    *animation = nullptr;
    animation_unschedule(running);
  }
}

// Picker and countdown layout: a heading on top and the activity name at the bottom, with the
// activity icon centered in between. Both screens must match for the icon hand-off.
#define WORKOUT_PICKER_HEADER_Y PBL_IF_ROUND_ELSE(18, -2)
#define WORKOUT_PICKER_HEADER_H 26

static inline int workout_picker_footer_y(GSize size) {
  return size.h - PBL_IF_ROUND_ELSE(72, 57);
}

static inline GRect workout_picker_icon_area(GSize size) {
  const int top = WORKOUT_PICKER_HEADER_Y + WORKOUT_PICKER_HEADER_H;
  return GRect(0, top, size.w, workout_picker_footer_y(size) - top);
}

static inline GRect workout_picker_icon_rect(GSize size, GSize icon_size) {
  const GRect area = workout_picker_icon_area(size);
  return GRect(area.origin.x + (area.size.w - icon_size.w) / 2,
               area.origin.y + (area.size.h - icon_size.h) / 2, icon_size.w, icon_size.h);
}

void workout_draw_picker_heading(GContext *ctx, int w, int y, const char *text);

void workout_draw_picker_name(GContext *ctx, int w, int y, const char *name);
