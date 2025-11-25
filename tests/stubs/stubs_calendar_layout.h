/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/calendar_layout.h"
#include "util/attributes.h"

LayoutLayer * WEAK calendar_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool WEAK calendar_layout_verify(bool existing_attributes[]) {
  return false;
}
