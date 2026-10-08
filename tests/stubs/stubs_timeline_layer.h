/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <apps/system/timeline/layer.h>

uint16_t PBL_WEAK timeline_layer_get_ideal_sidebar_width(void) {
  return 0;
}

uint16_t PBL_WEAK timeline_layer_get_sidebar_arrow_width(void) {
  return 0;
}
