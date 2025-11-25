/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/weather_layout.h"
#include "util/attributes.h"

LayoutLayer * WEAK weather_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool WEAK weather_layout_verify(bool existing_attributes[]) {
  return false;
}
