/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "applib/graphics/gtypes.h"
#include "applib/graphics/gpath.h"
#include "applib/ui/layer.h"

typedef struct PathLayer {
  Layer layer;
  GPath path;
  GColor stroke_color;
  GColor fill_color;
} PathLayer;

void path_layer_init(PathLayer *path_layer, const GPathInfo *path_info);

void path_layer_deinit(PathLayer *path_layer, const GPathInfo *path_info);

void path_layer_set_stroke_color(PathLayer *path_layer, GColor color);

void path_layer_set_fill_color(PathLayer *path_layer, GColor color);

//! Gets the "root" Layer of the path layer, which is the parent for the sub-
//! layers used for its implementation.
//! @param path_layer Pointer to the PathLayer for which to get the "root" Layer
//! @return The "root" Layer of the path layer.
//! @internal
//! @note The result is always equal to `(Layer *) path_layer`.
Layer* path_layer_get_layer(const PathLayer *path_layer);

