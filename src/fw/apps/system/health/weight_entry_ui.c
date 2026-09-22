/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "weight_entry_ui.h"

#include "applib/fonts/fonts.h"
#include "applib/ui/action_bar_layer.h"
#include "board/display.h"
#include "pbl/util/math.h"

#include <inttypes.h>
#include <stdio.h>

#define HEALTH_Y_OFFSET ((DISP_ROWS - LEGACY_2X_DISP_ROWS) / 2)
#define ENTRY_BACKGROUND_COLOR PBL_IF_COLOR_ELSE(GColorCeleste, GColorWhite)
#define ENTRY_ACCENT_COLOR     PBL_IF_COLOR_ELSE(GColorTiffanyBlue, GColorBlack)
#define ENTRY_VALUE_COLOR      PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack)
#define ENTRY_METADATA_COLOR   PBL_IF_COLOR_ELSE(GColorMidnightGreen, GColorBlack)
#define ENTRY_NEIGHBOR_COLOR   PBL_IF_COLOR_ELSE(GColorCadetBlue, GColorDarkGray)

static void prv_format_value(char *buffer, size_t buffer_size, int32_t value_tenths) {
  snprintf(buffer, buffer_size, "%" PRId32 ".%" PRId32, value_tenths / 10,
           value_tenths % 10);
}

static void prv_draw_current_value(GContext *ctx, const GRect *content_bounds,
                                   int32_t value_tenths, const char *unit,
                                   const char *value_font_key, int16_t y) {
  char value[16];
  prv_format_value(value, sizeof(value), value_tenths);
  GFont value_font = fonts_get_system_font(value_font_key);
  GFont unit_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  const GRect measure_frame = GRect(0, 0, content_bounds->size.w, 48);
  const int16_t value_width =
      app_graphics_text_layout_get_content_size(value, value_font, measure_frame,
                                                GTextOverflowModeTrailingEllipsis,
                                                GTextAlignmentLeft)
          .w;
  const int16_t unit_width =
      app_graphics_text_layout_get_content_size(unit, unit_font, measure_frame,
                                                GTextOverflowModeTrailingEllipsis,
                                                GTextAlignmentLeft)
          .w;
  const int16_t value_x = content_bounds->origin.x + (content_bounds->size.w - value_width) / 2;
  graphics_context_set_text_color(ctx, ENTRY_VALUE_COLOR);
  graphics_draw_text(ctx, value, value_font, GRect(value_x, y, value_width, 48),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, ENTRY_METADATA_COLOR);
  graphics_draw_text(ctx, unit, unit_font,
                     GRect(value_x + value_width + 4, y + 16, unit_width, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

void health_weight_entry_ui_draw(GContext *ctx, const GRect *bounds, int32_t value_tenths,
                                 int32_t min_tenths, int32_t max_tenths, const char *title,
                                 const char *unit, const char *value_font_key) {
  graphics_context_set_fill_color(ctx, ENTRY_BACKGROUND_COLOR);
  graphics_fill_rect(ctx, bounds);

  const GRect content_bounds = GRect(0, 0, bounds->size.w - ACTION_BAR_WIDTH, bounds->size.h);
  GRect frame = GRect(0, 16 + HEALTH_Y_OFFSET / 3, content_bounds.size.w, 30);
  graphics_context_set_text_color(ctx, ENTRY_METADATA_COLOR);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char neighbor[16];
  frame.origin.y = 30 + HEALTH_Y_OFFSET;
  frame.size.h = 34;
  graphics_context_set_text_color(ctx, ENTRY_NEIGHBOR_COLOR);
  prv_format_value(neighbor, sizeof(neighbor), MIN(max_tenths, value_tenths + 1));
  graphics_draw_text(ctx, neighbor, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  const int16_t current_y = 72 + HEALTH_Y_OFFSET;
  const int16_t horizontal_inset = PBL_IF_ROUND_ELSE(22, 9);
  GRect selection_frame =
      GRect(horizontal_inset, current_y - 4, content_bounds.size.w - 2 * horizontal_inset, 54);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_round_rect(ctx, &selection_frame, 10, GCornersAll);
  graphics_context_set_stroke_color(ctx, ENTRY_ACCENT_COLOR);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_round_rect(ctx, &selection_frame, 10);
  prv_draw_current_value(ctx, &content_bounds, value_tenths, unit, value_font_key, current_y);

  frame.origin.y = 124 + HEALTH_Y_OFFSET;
  graphics_context_set_text_color(ctx, ENTRY_NEIGHBOR_COLOR);
  prv_format_value(neighbor, sizeof(neighbor), MAX(min_tenths, value_tenths - 1));
  graphics_draw_text(ctx, neighbor, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}
