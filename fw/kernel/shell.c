/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <pbl/shell/shell.h>

#include <errno.h>
#include <string.h>

#include "pbl/services/runlevel.h"
#include "pbl/util/size.h"

PBL_SHELL_SUBCMD_SET_CREATE(sub_sys);
PBL_SHELL_CMD_REGISTER(sys, sub_sys, "System control", NULL);

static const char *s_runlevel_names[] = {
#define RUNLEVEL(number, name) [number] = #name,
#include "pbl/services/runlevel.def"
#undef RUNLEVEL
};

static void prv_list_runlevels(const struct pbl_shell *sh) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_runlevel_names); ++i) {
    pbl_shell_print(sh, "%u - %s", (unsigned int)i, s_runlevel_names[i]);
  }
}

static int prv_cmd_runlevel(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (strcmp(argv[1], "list") == 0) {
    prv_list_runlevels(sh);
    return 0;
  }

  long runlevel;
  if (pbl_shell_strtol(argv[1], &runlevel) != 0 || runlevel < 0 || runlevel >= RunLevel_COUNT) {
    pbl_shell_error(sh, "invalid runlevel '%s', choices:", argv[1]);
    prv_list_runlevels(sh);
    return -EINVAL;
  }

  pbl_shell_print(sh, "Switching to runlevel %s", s_runlevel_names[runlevel]);
  services_set_runlevel((RunLevel)runlevel);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, runlevel, NULL, "Set the runlevel <n|list>", prv_cmd_runlevel, 2, 0);

#endif
