/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "health_ui.h"

#include "applib/pbl_std/pbl_std.h"
#include "board/display.h"
#include "services/common/i18n/i18n.h"
#include "util/string.h"

// Compile-time display offset calculations
#define HEALTH_X_OFFSET ((DISP_COLS - LEGACY_2X_DISP_COLS) / 2)
#define HEALTH_Y_OFFSET ((DISP_ROWS - LEGACY_2X_DISP_ROWS) / 2)
#define HEALTH_INSET (18 + HEALTH_X_OFFSET / 5)

void health_ui_draw_text_in_box(GContext *ctx, const char *text, const GRect drawing_bounds,
                                const int16_t y_offset, const GFont small_font, GColor box_color,
                                GColor text_color) {
  const uint8_t text_height = fonts_get_font_height(small_font);
  const GTextOverflowMode overflow_mode = GTextOverflowModeFill;
  const GTextAlignment alignment = GTextAlignmentCenter;

  const GRect text_box = GRect(drawing_bounds.origin.x, y_offset,
                               drawing_bounds.size.w, text_height);

  GRect text_fill_box = text_box;
  text_fill_box.size = app_graphics_text_layout_get_content_size(
      text, small_font, text_box, overflow_mode, alignment);
  text_fill_box.origin.x += ((drawing_bounds.size.w - text_fill_box.size.w) / 2);

  // add a 3 px border (get content size already adds 1 px)
  text_fill_box = grect_inset(text_fill_box, GEdgeInsets(-2));

  // get content size adds 5 to the height, and the y offset is too high by a px (+ the 5px)
  const int height_correction = 5;
  text_fill_box.size.h -= height_correction;
  text_fill_box.origin.y += height_correction + 1;

  if (!gcolor_equal(box_color, GColorClear)) {
    graphics_context_set_fill_color(ctx, box_color);
    graphics_fill_rect(ctx, &text_fill_box);
  }

  if (!gcolor_equal(text_color, GColorClear)) {
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx, text, small_font, text_box, overflow_mode, alignment, NULL);
  }
}

void health_ui_render_typical_text_box(GContext *ctx, Layer *layer, const char *value_text) {
  time_t now = rtc_get_time();
  struct tm time_tm;
  localtime_r(&now, &time_tm);
  char weekday[8];
  strftime(weekday, sizeof(weekday), "%a", &time_tm);
  toupper_str(weekday);

  char typical_text[32];
  snprintf(typical_text, sizeof(typical_text), i18n_get("TYPICAL %s", layer), weekday);

  const int y = PBL_IF_RECT_ELSE(PBL_IF_BW_ELSE(122, 120), 125) + HEALTH_Y_OFFSET;
  GRect rect = GRect(0, y, layer->bounds.size.w, PBL_IF_RECT_ELSE(35, 36));
#if PBL_RECT
  rect = grect_inset(rect, GEdgeInsets(0, HEALTH_INSET));
#endif

  const GColor bg_color = PBL_IF_COLOR_ELSE(GColorYellow, GColorBlack);
  const GColor text_color = PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite);
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_round_rect(ctx, &rect, 3, GCornersAll);

  rect.origin.y -= PBL_IF_RECT_ELSE(3, 2);
  // Restrict the rect to draw one line at a time to prevent them from wrapping into each other
  rect.size.h = 16;

  graphics_context_set_text_color(ctx, text_color);

  graphics_draw_text(ctx, typical_text, font, rect,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  rect.origin.y += 16;

  graphics_draw_text(ctx, value_text, font, rect,
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}
