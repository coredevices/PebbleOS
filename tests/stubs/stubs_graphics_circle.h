/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics_circle.h"

void graphics_circle_quadrant_draw(GContext *ctx, GPoint p, uint16_t radius,
                                   GCornerMask quadrant) {}

void graphics_circle_quadrant_fill_non_aa(GContext *ctx, GPoint p, uint16_t radius,
                                          GCornerMask quadrant) {}

void graphics_internal_circle_quadrant_fill_aa(GContext *ctx, GPoint p,
                                               uint16_t radius, GCornerMask quadrant) {}
