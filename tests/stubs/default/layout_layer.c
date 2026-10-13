/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/timeline/layout_layer.h>

PBL_WEAK LayoutLayer *layout_create(LayoutId id, const LayoutLayerConfig *config) {
  return nullptr;
}

PBL_WEAK bool layout_verify(bool existing_attributes[], LayoutId id) {
  return true;
}

PBL_WEAK GSize layout_get_size(GContext *ctx, LayoutLayer *layout) {
  return GSize(0, 0);
}

PBL_WEAK void layout_destroy(LayoutLayer *layout) {
}
