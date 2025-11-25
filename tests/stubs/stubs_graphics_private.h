/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics_private.h"

void graphics_private_set_pixel(GContext* ctx, GPoint point) {}

void graphics_private_draw_horizontal_line_integral(GContext *ctx, GBitmap *framebuffer, int16_t y,
                                                    int16_t x1, int16_t x2, GColor color) {}
