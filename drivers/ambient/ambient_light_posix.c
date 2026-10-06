/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient_light.h>

#include "board/board.h"

static uint32_t s_dark_threshold;

void ambient_light_init(void) {
  s_dark_threshold = BOARD_CONFIG.ambient_light_dark_threshold;
  ambient_light_common_init();
}

uint32_t ambient_light_get_light_level(void) {
  return 0;
}

void ambient_light_driver_set_state(bool active, bool sampling) {
}

uint32_t ambient_light_get_dark_threshold(void) {
  return s_dark_threshold;
}

void ambient_light_set_dark_threshold(uint32_t new_threshold) {
  s_dark_threshold = new_threshold;
}

bool ambient_light_is_light(void) {
  return false;
}

AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelUnknown;
}
