/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "apps/system/timeline/layer.h"
#include "pbl/kernel/compiler.h"

uint16_t PBL_WEAK timeline_layer_get_ideal_sidebar_width(void) {
  return 0;
}

bool PBL_WEAK timeline_layer_sidebar_is_on_right(void) {
  return true;
}

int16_t PBL_WEAK timeline_layer_get_icon_outer_inset(void) {
  return 0;
}

int16_t PBL_WEAK timeline_layer_get_pin_text_origin_x(void) {
  return 0;
}
