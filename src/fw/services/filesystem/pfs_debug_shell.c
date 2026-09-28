/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW) && !defined(CONFIG_RELEASE)

#include <pbl/logging/logging.h>
#include <pbl/shell/shell.h>
#include <pbl/task_wdt/task_wdt.h>

#include "pbl/services/filesystem/pfs.h"

#include <errno.h>
#include <stdio.h>

// Create a lot of files and delete only a few of them, to fragment the filesystem
static int prv_cmd_litter(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long number;
  unsigned long size;
  char name[20];

  if (pbl_shell_strtoul(argv[1], &number) != 0) {
    pbl_shell_error(sh, "invalid number '%s'", argv[1]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[2], &size) != 0) {
    pbl_shell_error(sh, "invalid size '%s'", argv[2]);
    return -EINVAL;
  }

  for (unsigned long i = 0; i < number; i++) {
    snprintf(name, sizeof(name), "litter%lu", i);
    PBL_LOG_DBG("Creating %s", name);
    int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, size);
    if (i % 5 == 0) {
      pfs_close_and_remove(fd);
      PBL_LOG_DBG("Removed %s", name);
    } else {
      pfs_close(fd);
      PBL_LOG_DBG("Closed %s", name);
    }

    pbl_task_wdt_feed_self();
  }

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_pfs, litter, NULL, "Fragment the filesystem <files> <size>",
                     prv_cmd_litter, 3, 0);

#endif
