/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient_light.h>

#include <board/board.h>
#include <system/passert.h>

#include <inttypes.h>

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
  PBL_ASSERTN(new_threshold <= AMBIENT_LIGHT_LEVEL_MAX);
  s_dark_threshold = new_threshold;
}

bool ambient_light_is_light(void) {
  return ambient_light_level_to_lux(ambient_light_get_light_level()) > s_dark_threshold;
}

AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelUnknown;
}

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_als_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "%" PRIu32, ambient_light_get_light_level());
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_als, read, NULL, "Read the raw light level", prv_cmd_als_read, 0, 0);
#endif
