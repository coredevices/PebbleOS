/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "peek_animations.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "util/size.h"

#define LINE_WIDTH (2)
#define LINE_SPACING (10)

static void prv_draw_vertical_lines(GContext *ctx, unsigned int num_lines,
                                    const uint16_t *offsets_y, const uint16_t *heights,
                                    unsigned int width, unsigned int spacing, GPoint offset) {
  for (int i = 0; i < (int)num_lines; i++) {
    GRect box = {
      .origin = gpoint_add(GPoint((spacing + width) * i - width,
                                  offsets_y ? offsets_y[i] : 0), offset),
      .size = { width, heights[i] },
    };
    graphics_fill_rect(ctx, &box);
  }
}

void peek_animations_draw_compositor_foreground_speed_lines(GContext *ctx, GPoint offset) {
  static const uint16_t s_upper_heights[] = { 48, 73, 78, 48, 48, 48, 61, 48 };
  prv_draw_vertical_lines(ctx, ARRAY_LENGTH(s_upper_heights), NULL /* offsets_y */,
                          s_upper_heights, LINE_WIDTH, LINE_SPACING, offset);

  static const uint16_t s_lower_offsets_y[] = { 24, 24, 0, 19, 7, 0, 0, 24 };
  static const uint16_t s_lower_heights[] = { 48, 48, 72, 53, 65, 72, 72, 48 };
  offset.y += 90;
  prv_draw_vertical_lines(ctx, ARRAY_LENGTH(s_lower_heights), s_lower_offsets_y, s_lower_heights,
                          LINE_WIDTH, LINE_SPACING, offset);
}

void peek_animations_draw_compositor_background_speed_lines(GContext *ctx, GPoint offset) {
  static const uint16_t s_heights[] = { 0, DISP_ROWS, DISP_ROWS, 0, 0, 0, DISP_ROWS };
  prv_draw_vertical_lines(ctx, ARRAY_LENGTH(s_heights), NULL /* offsets_y */, s_heights,
                          LINE_WIDTH, LINE_SPACING, offset);
}

void peek_animations_draw_timeline_speed_lines(GContext *ctx, GPoint offset) {
  static const uint16_t s_upper_offsets_y[] = { 12, 0, 0, 12, 12, 12, 12, 12 };
  static const uint16_t s_upper_heights[] = { 53, 65, 65, 53, 53, 53, 53, 53 };
  prv_draw_vertical_lines(ctx, ARRAY_LENGTH(s_upper_heights), s_upper_offsets_y, s_upper_heights,
                          LINE_WIDTH, LINE_SPACING, offset);

  static const uint16_t s_lower_offsets_y[] = { 5, 5, 0, 0, 5, 5, 5, 5 };
  static const uint16_t s_lower_heights[] = { 53, 87, 87, 53, 53, 53, 53, 53 };
  offset.y += 65;
  prv_draw_vertical_lines(ctx, ARRAY_LENGTH(s_lower_heights), s_lower_offsets_y, s_lower_heights,
                          LINE_WIDTH, LINE_SPACING, offset);
}
