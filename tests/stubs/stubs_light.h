/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

void PBL_WEAK light_enable_interaction(void) {
}
void PBL_WEAK light_system_color_request(void) {
}
void PBL_WEAK light_system_color_release(void) {
}

static bool s_light_enabled;
void PBL_WEAK light_enable(bool enable) {
  s_light_enabled = enable;
}
bool PBL_WEAK light_is_on(void) {
  return s_light_enabled;
}

static bool s_breathe_active;
void PBL_WEAK light_start_charge_breathe(void) {
  s_breathe_active = true;
}
void PBL_WEAK light_stop_charge_breathe(void) {
  s_breathe_active = false;
}
