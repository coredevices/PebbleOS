/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/health_layout.h"
#include "util/attributes.h"

LayoutLayer * WEAK health_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool WEAK health_layout_verify(bool existing_attributes[]) {
  return false;
}
