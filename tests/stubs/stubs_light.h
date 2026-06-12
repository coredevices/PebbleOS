/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

void WEAK light_enable_interaction(void) {}
void WEAK light_system_color_request(void) {}
void WEAK light_system_color_release(void) {}

static bool s_light_enabled;
void WEAK light_enable(bool enable) {
  s_light_enabled = enable;
}
bool WEAK light_is_on(void) {
  return s_light_enabled;
}
