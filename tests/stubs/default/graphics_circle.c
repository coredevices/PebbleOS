/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/graphics_circle.h>

PBL_WEAK void graphics_circle_quadrant_draw(GContext *ctx, GPoint p, uint16_t radius,
                                            GCornerMask quadrant) {
}

PBL_WEAK void graphics_circle_quadrant_fill_non_aa(GContext *ctx, GPoint p, uint16_t radius,
                                                   GCornerMask quadrant) {
}

PBL_WEAK void graphics_internal_circle_quadrant_fill_aa(GContext *ctx, GPoint p, uint16_t radius,
                                                        GCornerMask quadrant) {
}
