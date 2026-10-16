/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/ambient_light.h>

#include <board/board.h>
#include <system/passert.h>

void ambient_light_init(void) {
  ambient_light_common_init();
}

uint32_t ambient_light_get_light_level(void) {
  return 0;
}

void ambient_light_driver_set_state(bool active, bool sampling) {
}

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_als_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "%" PRIu32, ambient_light_get_light_level());
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_als, read, nullptr, "Read the raw light level", prv_cmd_als_read, 0, 0);
#endif
