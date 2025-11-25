/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/timeline/layout_layer.h"

LayoutLayer *layout_create(LayoutId id, const LayoutLayerConfig *config) {
  return NULL;
}

bool layout_verify(bool existing_attributes[], LayoutId id) {
  return true;
}

GSize layout_get_size(GContext *ctx, LayoutLayer *layout) {
  return GSize(0, 0);
}

void layout_destroy(LayoutLayer *layout) {
}
