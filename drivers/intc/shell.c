/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/intc.h>
#include <pbl/shell/shell.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_intc_class, dev) {
    const struct pbl_intc *intc = PBL_CONTAINER_OF(dev, const struct pbl_intc, dev);

    pbl_shell_print(sh, "%s: %u lines%s", pbl_device_name(&intc->dev), intc->num_irqs,
                    pbl_device_is_ready(&intc->dev) ? "" : " (not ready)");

    for (uint16_t hwirq = 0U; hwirq < intc->num_irqs; hwirq++) {
      const struct pbl_intc_line *line = &intc->lines[hwirq];

      if (line->handler == nullptr) {
        continue;
      }

      pbl_shell_print(sh, "  %3u %-8s %p(%p)", hwirq,
                      (intc->enabled[hwirq / 32U] & PBL_BIT(hwirq % 32U)) ? "enabled" : "disabled",
                      (void *)(uintptr_t)line->handler, line->arg);
    }
  }

  return 0;
}

static const struct pbl_shell_cmd sub_irq[] = {
  PBL_SHELL_CMD(list, nullptr, "List the requested lines of every controller", prv_cmd_list),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(irq, sub_irq, "Interrupt controllers", nullptr);
