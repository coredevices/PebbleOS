/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "profiler_priv.h"

#include <pbl/profiler/profiler.h>
#include <pbl/shell/shell.h>

static int prv_cmd_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_profiler_reset();
  pbl_profiler_start();
  return 0;
}

static void prv_emit(const char *line, void *ctx) {
  pbl_shell_print(ctx, "%s", line);
}

static int prv_cmd_stop(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_profiler__stats(prv_emit, (void *)sh);
  return 0;
}

static const struct pbl_shell_cmd sub_profiler[] = {
  PBL_SHELL_CMD(start, nullptr, "Reset and start profiling", prv_cmd_start),
  PBL_SHELL_CMD(stop, nullptr, "Stop profiling and print the stats", prv_cmd_stop),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(profiler, sub_profiler, "Profiler", nullptr);
