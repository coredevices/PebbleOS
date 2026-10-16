/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient_light.h>

#include <board/board.h>

void ambient_light_init(void) {
  ambient_light_common_init();
}

uint32_t ambient_light_get_light_level(void) {
  return 0;
}

void ambient_light_driver_set_state(bool active, bool sampling) {
}
