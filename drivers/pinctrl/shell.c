/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/pinctrl.h>
#include <pbl/shell/shell.h>
#include <pbl/util/misc.h>

static int prv_cmd_show(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_pinctrl_class, dev) {
    const struct pbl_pinctrl_controller *ctrl =
        container_of(dev, const struct pbl_pinctrl_controller, dev);

    pbl_shell_print(sh, "%s: %u pads", ctrl->dev.name, ctrl->num_pads);

    if (ctrl->ops->get == nullptr) {
      continue;
    }

    for (uint16_t pad = 0U; pad < ctrl->num_pads; pad++) {
      uint32_t val;
      int ret = ctrl->ops->get(ctrl, pad, &val);

      if (ret == 0) {
        pbl_shell_print(sh, "  %3u 0x%08" PRIx32, pad, val);
      }
    }
  }

  return 0;
}

static const struct pbl_shell_cmd sub_pinctrl[] = {
  PBL_SHELL_CMD(show, nullptr, "Dump the pad registers of every controller", prv_cmd_show),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(pinctrl, sub_pinctrl, "Pin controllers", nullptr);
