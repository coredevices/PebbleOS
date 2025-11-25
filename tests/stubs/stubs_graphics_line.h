/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics_line.h"

void graphics_draw_line(GContext* ctx, GPoint p0, GPoint p1) {}

void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1) {}

void graphics_line_draw_stroked_aa(GContext* ctx, GPoint p0, GPoint p1, uint8_t stroke_width) {}

void graphics_line_draw_stroked_non_aa(GContext* ctx, GPoint p0, GPoint p1, uint8_t stroke_width) {}
