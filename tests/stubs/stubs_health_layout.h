/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/health_layout.h"
#include "pbl/kernel/compiler.h"

LayoutLayer *PBL_WEAK health_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK health_layout_verify(bool existing_attributes[]) {
  return false;
}
