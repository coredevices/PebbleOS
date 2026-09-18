/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "popups/mic_banner.h"
#include "pbl/util/attributes.h"

void WEAK mic_banner_init(void) {
}

void WEAK mic_banner_show(void) {
}

void WEAK mic_banner_hide(void) {
}

bool WEAK mic_banner_is_visible(void) {
  return false;
}

int16_t WEAK mic_banner_get_obstruction_origin_y(void) {
  return DISP_ROWS;
}
