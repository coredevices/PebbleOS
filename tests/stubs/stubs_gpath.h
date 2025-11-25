/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gpath.h"

void gpath_init(GPath *path, const GPathInfo *init) {}

void gpath_move_to(GPath *path, GPoint point) {}

void gpath_draw_stroke(GContext* ctx, GPath* path, bool open) {}

void gpath_draw_filled(GContext* ctx, GPath *path) {}

void gpath_fill_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points) {}

void gpath_draw_outline_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points,
                                         bool open) {}

GRect gpath_outer_rect(GPath *path) {
  return (GRect) {};
}
