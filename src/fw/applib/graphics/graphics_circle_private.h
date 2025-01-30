/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
