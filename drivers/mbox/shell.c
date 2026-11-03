/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/mbox.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_mbox_class, dev) {
    const struct pbl_mbox_controller *ctrl =
        container_of(dev, const struct pbl_mbox_controller, dev);

    if (!pbl_device_is_ready(dev)) {
      pbl_shell_print(sh, "%s (not ready)", dev->name);
      continue;
    }

    pbl_shell_print(sh, "%s", dev->name);
    for (uint8_t idx = 0U; idx < ctrl->num_chans; idx++) {
      const struct pbl_mbox_chan_state *st = &ctrl->chans[idx];

      if (st->cl == nullptr && st->tx_count == 0U && st->rx_count == 0U) {
        continue;
      }

      pbl_shell_print(sh, "  %u: %s tx %" PRIu32 " rx %" PRIu32 "%s", idx,
                      (st->cl != nullptr) ? "requested" : "free     ", st->tx_count, st->rx_count,
                      st->tx_active ? " (tx in flight)" : "");
    }
  }

  return 0;
}

static int prv_cmd_send(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev = pbl_device_find(&pbl_mbox_class, argv[1]);
  struct pbl_mbox_chan chan = {};
  int ret;

  if (dev == nullptr) {
    pbl_shell_error(sh, "unknown controller '%s'", argv[1]);
    return -ENODEV;
  }

  chan.ctrl = container_of(dev, const struct pbl_mbox_controller, dev);
  for (size_t i = 2U; i < argc; i++) {
    unsigned long cell;

    if (pbl_shell_strtoul(argv[i], &cell) != 0) {
      pbl_shell_error(sh, "invalid cell '%s'", argv[i]);
      return -EINVAL;
    }

    chan.cells[i - 2U] = (uint32_t)cell;
  }

  ret = pbl_mbox_send(&chan, nullptr);
  if (ret != 0) {
    pbl_shell_error(sh, "send failed (%d)", ret);
  }

  return ret;
}

static const struct pbl_shell_cmd sub_mbox[] = {
  PBL_SHELL_CMD(list, nullptr, "List the controllers and their active channels", prv_cmd_list),
  PBL_SHELL_CMD_ARG(send, nullptr, "Ring a requested channel <ctrl> <cell> [cell]", prv_cmd_send, 3,
                    PBL_MBOX_MAX_CELLS - 1U),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(mbox, sub_mbox, "Mailbox controllers", nullptr);
