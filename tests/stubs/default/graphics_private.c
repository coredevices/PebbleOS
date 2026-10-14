/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/graphics_private.h>

PBL_WEAK void graphics_private_set_pixel(GContext *ctx, GPoint point) {
}

PBL_WEAK void graphics_private_draw_horizontal_line_integral(GContext *ctx, GBitmap *framebuffer,
                                                             int16_t y, int16_t x1, int16_t x2,
                                                             GColor color) {
}
