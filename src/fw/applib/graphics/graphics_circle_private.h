/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "gtypes.h"

// For arc/radial fill algorithms
#define QUADRANTS_NUM 4 // Just in case of fluctuation
#define QUADRANT_ANGLE (TRIG_MAX_ANGLE / QUADRANTS_NUM)

static GCornerMask radius_quadrants[QUADRANTS_NUM] =
{ GCornerTopRight, GCornerBottomRight, GCornerBottomLeft, GCornerTopLeft };

typedef struct {
  int32_t angle;
  GCornerMask quadrant;
} EllipsisPartDrawConfig;

typedef struct {
  EllipsisPartDrawConfig start_quadrant;
  GCornerMask full_quadrants;
  EllipsisPartDrawConfig end_quadrant;
} EllipsisDrawConfig;

typedef struct {
  GCornerMask mask;
  int8_t x_mul;
  int8_t y_mul;
} GCornerMultiplier;

#if PBL_COLOR
static GCornerMultiplier quadrant_mask_mul[] = {
  {GCornerTopRight,     1, -1},
  {GCornerBottomRight,  1,  1},
  {GCornerBottomLeft,  -1,  1},
  {GCornerTopLeft,     -1, -1}
};
#endif

T_STATIC EllipsisDrawConfig prv_calc_draw_config_ellipsis(int32_t angle_start, int32_t angle_end);

void prv_fill_oval_quadrant(GContext *ctx, GPoint point,
                            uint16_t outer_radius_x, uint16_t outer_radius_y,
                            uint16_t inner_radius_x, uint16_t inner_radius_y,
                            GCornerMask quadrant);
