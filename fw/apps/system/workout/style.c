/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "style.h"

#include <pbl/util/size.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/gdraw_command_image.h>
#include <applib/graphics/gdraw_command_list.h>
#include <applib/graphics/gdraw_command_transforms.h>
#include <applib/graphics/gpath.h>
#include <applib/graphics/text.h>
#include <resource/resource_ids.auto.h>

static bool prv_tint_icon_command(GDrawCommand *command, uint32_t index, void *context) {
  gdraw_command_replace_color(command, GColorWhite, GColorClear);
  gdraw_command_replace_color(command, GColorBlack, *(GColor *)context);
  return true;
}

KinoReel *workout_create_activity_icon(ActivitySessionType type) {
  KinoReel *icon = kino_reel_create_with_resource(
      type == ActivitySessionType_Open   ? RESOURCE_ID_WORKOUT_APP_WORKOUT
      : type == ActivitySessionType_Walk ? RESOURCE_ID_WORKOUT_APP_WALK
                                         : RESOURCE_ID_WORKOUT_APP_RUN);
  GDrawCommandImage *image = kino_reel_get_gdraw_command_image(icon);
  if (image) {
    GColor color = workout_activity_color(type);
    gdraw_command_list_iterate(gdraw_command_image_get_command_list(image), prv_tint_icon_command,
                               &color);
  }
  return icon;
}

void workout_fill_heart(GContext *ctx, GRect box, GColor color) {
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

void workout_draw_select_indicator(GContext *ctx, GSize size) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_oval(ctx, GRect(size.w - 5, (size.h - 26) / 2, 26, 26), GOvalScaleModeFitCircle);
}

void workout_draw_picker_heading(GContext *ctx, int w, int y, const char *text) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, y, w, WORKOUT_PICKER_HEADER_H), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, nullptr);
  const int side = PBL_IF_ROUND_ELSE(24, 0);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &GRect(side, y + WORKOUT_PICKER_HEADER_H - 1, w - 2 * side, 2));
}

void workout_draw_picker_name(GContext *ctx, int w, int y, const char *name) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, name, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(0, y + 1, w, 34), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, nullptr);
}
