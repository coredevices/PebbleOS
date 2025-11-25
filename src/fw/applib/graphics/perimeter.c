/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/perimeter.h"

#include "system/passert.h"
#include "util/math.h"

#if PBL_ROUND
static uint16_t prv_triangle_side(uint16_t hypotenuse, uint16_t side) {
  // third side of triangle based on pythagorean theorem
  return integer_sqrt(ABS(((uint32_t)hypotenuse * hypotenuse) - ((uint32_t)side * side)));
}

T_STATIC GRangeHorizontal perimeter_for_circle(GRangeVertical vertical_range, GPoint center,
                                      int32_t radius) {
  radius = MAX(0, radius);
  int32_t height = 0;
  int32_t width = 0;

  const int32_t top = center.y - radius;
  const int32_t bottom = center.y + radius;

  int32_t range_start = vertical_range.origin_y;
  int32_t range_end = vertical_range.origin_y + vertical_range.size_h;

  // Check if both top and bottom are outside but not surrounding the perimeter
  if ((range_start < top && range_end < top) ||
      (range_start > bottom && range_end > bottom)) {
    return (GRangeHorizontal){0, 0};
  }

  range_start = CLIP(range_start, top, bottom);
  range_end = CLIP(range_end, top, bottom);

  // height of triangle from center to range start
  height = ABS(center.y - range_start);
  const int32_t start_width = prv_triangle_side(radius, height);

  // height of triangle from center to range end
  height = ABS(center.y - range_end);
  const int32_t end_width = prv_triangle_side(radius, height);

  width = MIN(start_width, end_width);

  return (GRangeHorizontal){.origin_x = center.x - width, .size_w = width * 2};
}

T_STATIC GRangeHorizontal perimeter_for_display_round(const GPerimeter *perimeter,
                                                      const GSize *ctx_size,
                                                      GRangeVertical vertical_range,
                                                      uint16_t inset) {
  const GRect frame = (GRect) { GPointZero, *ctx_size };
  const GPoint center = grect_center_point(&frame);
  const int32_t radius = grect_shortest_side(frame) / 2 - inset;
  return perimeter_for_circle(vertical_range, center, radius);
}
#endif

T_STATIC GRangeHorizontal perimeter_for_display_rect(const GPerimeter *perimeter,
                                                     const GSize *ctx_size,
                                                     GRangeVertical vertical_range,
                                                     uint16_t inset) {
  return (GRangeHorizontal){.origin_x = inset, .size_w = MAX(0, ctx_size->w - 2 * inset)};
}

const GPerimeter * const g_perimeter_for_display = &(const GPerimeter) {
  .callback = PBL_IF_RECT_ELSE(perimeter_for_display_rect, perimeter_for_display_round),
};
