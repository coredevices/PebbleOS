/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>

PBL_DEVICE_CLASS_DEFINE(pbl_clk_controller_class, "clk");

static struct pbl_clk_notifier *s_notifiers;

int pbl_clk_enable(const struct pbl_clk *clk) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  int ret = 0;

  PBL_ASSERTN(pbl_device_is_ready(&ctrl->dev));

  if (clk->id >= ctrl->num_clks) {
    return -EINVAL;
  }

  pbl_irq_lock();

  if (ctrl->use_counts[clk->id] == 0U && ctrl->ops->enable != nullptr) {
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

  if (ctrl->use_counts[clk->id] == 1U && ctrl->ops->disable != nullptr) {
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

int pbl_clk_get_rate(const struct pbl_clk *clk, uint64_t *rate_mhz) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  int ret;

  if (!pbl_device_is_ready(&ctrl->dev)) {
    return -ENODEV;
  }

  if (clk->id >= ctrl->num_clks) {
    return -EINVAL;
  }

  if (ctrl->ops->get_rate == nullptr) {
    return -ENOTSUP;
  }

  pbl_irq_lock();
  ret = ctrl->ops->get_rate(ctrl, clk->id, rate_mhz);
  pbl_irq_unlock();

  return ret;
}

void pbl_clk_notifier_register(struct pbl_clk_notifier *nb) {
  pbl_irq_lock();
  nb->next = s_notifiers;
  s_notifiers = nb;
  pbl_irq_unlock();
}

void pbl_clk_rate_changed(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t rate_mhz) {
  struct pbl_clk_notifier *nb;

  pbl_irq_lock();
  nb = s_notifiers;
  pbl_irq_unlock();

  for (; nb != nullptr; nb = nb->next) {
    if (nb->clk.ctrl == ctrl && nb->clk.id == id) {
      nb->fn(nb, rate_mhz);
    }
  }
}
