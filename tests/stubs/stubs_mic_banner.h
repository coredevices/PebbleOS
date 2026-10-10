/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <popups/mic_banner.h>

void PBL_WEAK mic_banner_init(void) {
}

void PBL_WEAK mic_banner_show(void) {
}

void PBL_WEAK mic_banner_hide(void) {
}

bool PBL_WEAK mic_banner_is_visible(void) {
  return false;
}

int16_t PBL_WEAK mic_banner_get_obstruction_origin_y(void) {
  return DISP_ROWS;
}
