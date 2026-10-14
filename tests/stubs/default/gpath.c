/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/gpath.h>

PBL_WEAK void gpath_init(GPath *path, const GPathInfo *init) {
}

PBL_WEAK void gpath_move_to(GPath *path, GPoint point) {
}

PBL_WEAK void gpath_draw_stroke(GContext *ctx, GPath *path, bool open) {
}

PBL_WEAK void gpath_draw_filled(GContext *ctx, GPath *path) {
}

PBL_WEAK void gpath_fill_internal(GContext *ctx, const GPointUnaligned *points, size_t num_points) {
}

PBL_WEAK void gpath_draw_outline_internal(GContext *ctx, const GPointUnaligned *points,
                                          size_t num_points, bool open) {
}

PBL_WEAK void gpath_fill_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points) {
}

PBL_WEAK void gpath_draw_outline_precise_internal(GContext *ctx, GPointPrecise *points,
                                                  size_t num_points, bool open) {
}

PBL_WEAK GRect gpath_outer_rect(GPath *path) {
  return (GRect){};
}
