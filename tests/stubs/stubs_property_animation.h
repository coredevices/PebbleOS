/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/property_animation.h"
#include "util/attributes.h"

bool WEAK property_animation_from(PropertyAnimation *property_animation, void *from, size_t size,
                                  bool set) {
  return false;
}

void WEAK property_animation_update_grect(PropertyAnimation *property_animation,
                                          const uint32_t distance_normalized) {}

PropertyAnimation *WEAK property_animation_create_layer_bounds(
    struct Layer *layer, GRect *from_bounds, GRect *to_bounds) {
  return NULL;
}

PropertyAnimation *WEAK property_animation_create_bounds_origin(
    struct Layer *layer, GPoint *from, GPoint *to) {
  return NULL;
}
