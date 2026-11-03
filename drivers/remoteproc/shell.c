/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/remoteproc.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_rproc_class, dev) {
    const struct pbl_rproc *rproc = container_of(dev, const struct pbl_rproc, dev);

    if (!pbl_device_is_ready(dev)) {
      pbl_shell_print(sh, "%s (not ready)", dev->name);
      continue;
    }

    pbl_shell_print(sh, "%s: %s (%u)", dev->name,
                    pbl_rproc_is_running(rproc) ? "running" : "offline", rproc->state->power);
  }

  return 0;
}

static const struct pbl_rproc *prv_find(const struct pbl_shell *sh, const char *name) {
  const struct pbl_device *dev = pbl_device_find(&pbl_rproc_class, name);

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown remote processor '%s'", name);
    return nullptr;
  }

  return container_of(dev, const struct pbl_rproc, dev);
}

static int prv_cmd_boot(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_rproc *rproc = prv_find(sh, argv[1]);
  int ret;

  if (rproc == nullptr) {
    return -ENODEV;
  }

  ret = pbl_rproc_boot(rproc);
  if (ret != 0) {
    pbl_shell_error(sh, "boot failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_shutdown(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_rproc *rproc = prv_find(sh, argv[1]);

  if (rproc == nullptr) {
    return -ENODEV;
  }

  pbl_rproc_shutdown(rproc);

  return 0;
}

static const struct pbl_shell_cmd sub_rproc[] = {
  PBL_SHELL_CMD(list, nullptr, "List the remote processors and their boot counts", prv_cmd_list),
  PBL_SHELL_CMD_ARG(boot, nullptr, "Boot <rproc>, or count one more boot", prv_cmd_boot, 2, 0),
  PBL_SHELL_CMD_ARG(shutdown, nullptr, "Release a boot of <rproc>", prv_cmd_shutdown, 2, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(rproc, sub_rproc, "Remote processors", nullptr);
