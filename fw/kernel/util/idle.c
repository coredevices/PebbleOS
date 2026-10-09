/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

static bool s_idle_allowed = true;

void idle_set_enabled(bool enable) {
  s_idle_allowed = enable;
}

bool idle_is_allowed(void) {
  return s_idle_allowed;
}

#ifdef CONFIG_SHELL
static int prv_cmd_force_active(const struct pbl_shell *sh, size_t argc, char **argv) {
  idle_set_enabled(false);
  return 0;
}

static int prv_cmd_resume_normal(const struct pbl_shell *sh, size_t argc, char **argv) {
  idle_set_enabled(true);
  return 0;
}

PBL_SHELL_SUBCMD_SET_CREATE(sub_sys_scheduler);
PBL_SHELL_SUBCMD_ADD(sub_sys, scheduler, sub_sys_scheduler, "Scheduler idle control", NULL, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys_scheduler, force_active, NULL, "Keep the CPU out of idle",
                     prv_cmd_force_active, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys_scheduler, resume_normal, NULL, "Allow the CPU to idle again",
                     prv_cmd_resume_normal, 0, 0);
#endif
