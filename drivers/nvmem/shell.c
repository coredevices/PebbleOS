/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/nvmem.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

#define READ_MAX 64U

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_nvmem_class, dev) {
    const struct pbl_nvmem *nvmem = container_of(dev, const struct pbl_nvmem, dev);

    pbl_shell_print(sh, "%s: %zu bytes, word %u, stride %u%s", dev->name, nvmem->size,
                    nvmem->word_size, nvmem->stride,
                    pbl_device_is_ready(dev) ? "" : " (not ready)");
  }

  return 0;
}

static int prv_cmd_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev = pbl_device_find(&pbl_nvmem_class, argv[1]);
  uint8_t buf[READ_MAX];
  unsigned long offset;
  unsigned long bytes;
  int ret;

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown device '%s'", argv[1]);
    return -ENODEV;
  }

  if (pbl_shell_strtoul(argv[2], &offset) != 0 || pbl_shell_strtoul(argv[3], &bytes) != 0 ||
      bytes > sizeof(buf)) {
    pbl_shell_error(sh, "invalid range");
    return -EINVAL;
  }

  ret = pbl_nvmem_device_read(container_of(dev, const struct pbl_nvmem, dev), offset, bytes, buf);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_hexdump(sh, buf, bytes);

  return 0;
}

static const struct pbl_shell_cmd sub_nvmem[] = {
  PBL_SHELL_CMD(list, nullptr, "List the memories", prv_cmd_list),
  PBL_SHELL_CMD_ARG(read, nullptr, "Read <device> <offset> <bytes>", prv_cmd_read, 4, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(nvmem, sub_nvmem, "Non-volatile memories", nullptr);
