/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_factory_mode.h"

#include <apps/prf/mfg_menu.h>
#include <board/board.h>
#include <kernel/event_loop.h>
#include <kernel/low_power.h>
#include <process_management/app_manager.h>

static bool s_mfg_mode = false;

static void prv_launch_mfg_app(void *data) {
  // Make sure we can launch our MFG app and subsequent apps.
  app_manager_set_minimum_run_level(ProcessAppRunLevelNormal);
  app_manager_launch_new_app(&(AppLaunchConfig){
    .md = mfg_menu_app_get_info(),
  });
}

void mfg_enter_mfg_mode(void) {
  if (!s_mfg_mode) {
    s_mfg_mode = true;

    low_power_exit();
  }
}

void mfg_enter_mfg_mode_and_launch_app(void) {
  if (!s_mfg_mode) {
    mfg_enter_mfg_mode();
    launcher_task_add_callback(prv_launch_mfg_app, NULL);
  }
}

bool mfg_is_mfg_mode(void) {
  return s_mfg_mode;
}

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_enter(const struct pbl_shell *sh, size_t argc, char **argv) {
  mfg_enter_mfg_mode_and_launch_app();
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_mfg, enter, NULL, "Enter manufacturing mode", prv_cmd_enter, 0, 0);
#endif
