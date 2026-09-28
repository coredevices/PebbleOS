/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RELEASE)

#include <pbl/drivers/display/sf32lb/display_jdi.h>
#include <pbl/shell/shell.h>

// The silent-loss timer should PBL_CROAK ~500ms later, with a coredump and a reboot.
static int prv_cmd_drop_complete(const struct pbl_shell *sh, size_t argc, char **argv) {
  display_jdi_test_drop_next_complete();
  pbl_shell_print(sh, "armed drop of next LCDC complete, PBL_CROAK in ~500ms");
  return 0;
}

static const struct pbl_shell_cmd sub_display[] = {
  PBL_SHELL_CMD(drop_complete, NULL, "Drop the next LCDC transfer-complete callback",
                prv_cmd_drop_complete),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(display, sub_display, "Display", NULL);

#endif
