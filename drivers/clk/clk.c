/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>

int pbl_clk_enable(const struct pbl_clk *clk) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  int ret = 0;

  PBL_ASSERTN(pbl_device_is_ready(&ctrl->dev));

  if (clk->id >= ctrl->num_clks) {
    return -EINVAL;
  }

  pbl_irq_lock();

  if (ctrl->use_counts[clk->id] == 0U) {
    ret = ctrl->ops->enable(ctrl, clk->id);
  }

  if (ret == 0) {
    PBL_ASSERTN(ctrl->use_counts[clk->id] < UINT8_MAX);
    ctrl->use_counts[clk->id]++;
  }

  pbl_irq_unlock();

  return ret;
}

int pbl_clk_disable(const struct pbl_clk *clk) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  int ret = 0;

  if (clk->id >= ctrl->num_clks) {
    return -EINVAL;
  }

  pbl_irq_lock();

  PBL_ASSERTN(ctrl->use_counts[clk->id] > 0U);

  if (ctrl->use_counts[clk->id] == 1U) {
    ret = ctrl->ops->disable(ctrl, clk->id);
  }

  if (ret == 0) {
    ctrl->use_counts[clk->id]--;
  }

  pbl_irq_unlock();

  return ret;
}

bool pbl_clk_is_enabled(const struct pbl_clk *clk) {
  return (clk->id < clk->ctrl->num_clks) && (clk->ctrl->use_counts[clk->id] > 0U);
}
