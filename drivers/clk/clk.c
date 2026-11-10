/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/clk.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>

PBL_DEVICE_CLASS_DEFINE(pbl_clk_controller_class, "clk");

// Deeper trees are a loop
#define MAX_DEPTH 16U

static struct pbl_clk_notifier *s_notifiers;

static bool prv_valid(const struct pbl_clk *clk) {
  return clk->ctrl != nullptr && clk->id < clk->ctrl->num_clks;
}

// A controller uses its own clocks while it comes up, as to apply its assigned clocks
static bool prv_up(const struct pbl_clk_controller *ctrl) {
  uint8_t status = ctrl->dev.state->status;

  return status == PBL_DEVICE_READY || status == PBL_DEVICE_INITIALIZING;
}

static bool prv_same(const struct pbl_clk *a, const struct pbl_clk *b) {
  return a->ctrl == b->ctrl && a->id == b->id;
}

static int prv_parent(const struct pbl_clk *clk, struct pbl_clk *parent) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  int ret;

  if (ctrl->ops->get_parent == nullptr) {
    return -ENOENT;
  }

  ret = ctrl->ops->get_parent(ctrl, clk->id, parent);
  if (ret == 0 && !prv_valid(parent)) {
    return -EINVAL;
  }

  return ret;
}

static int prv_disable(const struct pbl_clk *clk) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  struct pbl_clk parent;
  int ret;

  if (!prv_valid(clk)) {
    return -EINVAL;
  }

  PBL_ASSERTN(ctrl->use_counts[clk->id] > 0U);

  if (ctrl->use_counts[clk->id] > 1U) {
    ctrl->use_counts[clk->id]--;
    return 0;
  }

  if (ctrl->ops->disable != nullptr) {
    ret = ctrl->ops->disable(ctrl, clk->id);
    if (ret != 0) {
      return ret;
    }
  }

  ctrl->use_counts[clk->id] = 0U;

  if (prv_parent(clk, &parent) == 0) {
    (void)prv_disable(&parent);
  }

  return 0;
}

static int prv_enable(const struct pbl_clk *clk, uint8_t depth) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  struct pbl_clk parent;
  bool has_parent;
  int ret;

  if (!prv_valid(clk) || depth > MAX_DEPTH) {
    return -EINVAL;
  }

  if (ctrl->use_counts[clk->id] > 0U) {
    PBL_ASSERTN(ctrl->use_counts[clk->id] < UINT8_MAX);
    ctrl->use_counts[clk->id]++;
    return 0;
  }

  ret = prv_parent(clk, &parent);
  if (ret != 0 && ret != -ENOENT) {
    return ret;
  }

  has_parent = ret == 0;
  if (has_parent) {
    ret = prv_enable(&parent, depth + 1U);
    if (ret != 0) {
      return ret;
    }
  }

  if (ctrl->ops->enable != nullptr) {
    ret = ctrl->ops->enable(ctrl, clk->id);
    if (ret != 0) {
      if (has_parent) {
        (void)prv_disable(&parent);
      }
      return ret;
    }
  }

  ctrl->use_counts[clk->id] = 1U;

  return 0;
}

static int prv_get_rate(const struct pbl_clk *clk, uint64_t *rate_mhz, uint8_t depth) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  struct pbl_clk parent;
  uint64_t parent_mhz = 0U;
  int ret;

  if (!prv_valid(clk) || depth > MAX_DEPTH) {
    return -EINVAL;
  }

  if (!prv_up(ctrl)) {
    return -ENODEV;
  }

  ret = prv_parent(clk, &parent);
  if (ret == 0) {
    ret = prv_get_rate(&parent, &parent_mhz, depth + 1U);
    if (ret != 0) {
      return ret;
    }
  } else if (ret != -ENOENT) {
    return ret;
  } else if (ctrl->ops->get_rate == nullptr) {
    return -ENOTSUP;
  }

  if (ctrl->ops->get_rate == nullptr) {
    *rate_mhz = parent_mhz;
    return 0;
  }

  return ctrl->ops->get_rate(ctrl, clk->id, parent_mhz, rate_mhz);
}

int pbl_clk_enable(const struct pbl_clk *clk) {
  int ret;

  if (prv_valid(clk)) {
    PBL_ASSERTN(prv_up(clk->ctrl));
  }

  pbl_irq_lock();
  ret = prv_enable(clk, 0U);
  pbl_irq_unlock();

  return ret;
}

int pbl_clk_disable(const struct pbl_clk *clk) {
  int ret;

  pbl_irq_lock();
  ret = prv_disable(clk);
  pbl_irq_unlock();

  return ret;
}

bool pbl_clk_is_enabled(const struct pbl_clk *clk) {
  return prv_valid(clk) && (clk->ctrl->use_counts[clk->id] > 0U);
}

int pbl_clk_get_rate(const struct pbl_clk *clk, uint64_t *rate_mhz) {
  int ret;

  pbl_irq_lock();
  ret = prv_get_rate(clk, rate_mhz, 0U);
  pbl_irq_unlock();

  return ret;
}

int pbl_clk_get_parent(const struct pbl_clk *clk, struct pbl_clk *parent) {
  int ret;

  if (!prv_valid(clk)) {
    return -EINVAL;
  }

  pbl_irq_lock();
  ret = prv_parent(clk, parent);
  pbl_irq_unlock();

  return ret;
}

int pbl_clk_set_parent(const struct pbl_clk *clk, const struct pbl_clk *parent) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  struct pbl_clk old;
  bool has_old;
  bool enabled;
  int ret;

  if (!prv_valid(clk) || !prv_valid(parent)) {
    return -EINVAL;
  }

  if (ctrl->ops->set_parent == nullptr) {
    return -ENOTSUP;
  }

  pbl_irq_lock();

  has_old = prv_parent(clk, &old) == 0;
  if (has_old && prv_same(&old, parent)) {
    pbl_irq_unlock();
    return 0;
  }

  // An enabled clock holds a reference on its parent
  enabled = ctrl->use_counts[clk->id] > 0U;
  if (enabled) {
    ret = prv_enable(parent, 0U);
    if (ret != 0) {
      pbl_irq_unlock();
      return ret;
    }
  }

  ret = ctrl->ops->set_parent(ctrl, clk->id, parent);
  if (enabled && ret != 0) {
    (void)prv_disable(parent);
  } else if (enabled && has_old) {
    (void)prv_disable(&old);
  }

  pbl_irq_unlock();

  if (ret == 0) {
    pbl_clk_rate_changed(clk);
  }

  return ret;
}

int pbl_clk_set_rate(const struct pbl_clk *clk, uint64_t rate_mhz) {
  const struct pbl_clk_controller *ctrl = clk->ctrl;
  struct pbl_clk parent;
  uint64_t parent_mhz = 0U;
  int ret;

  if (!prv_valid(clk)) {
    return -EINVAL;
  }

  if (ctrl->ops->set_rate == nullptr) {
    return -ENOTSUP;
  }

  pbl_irq_lock();

  ret = prv_parent(clk, &parent);
  if (ret == 0) {
    ret = prv_get_rate(&parent, &parent_mhz, 0U);
  } else if (ret == -ENOENT) {
    ret = 0;
  }

  if (ret == 0) {
    ret = ctrl->ops->set_rate(ctrl, clk->id, rate_mhz, parent_mhz);
  }

  pbl_irq_unlock();

  if (ret == 0) {
    pbl_clk_rate_changed(clk);
  }

  return ret;
}

int pbl_clk_assign(const struct pbl_clk_assigned *assigned) {
  const struct pbl_clk_assigned *cur;
  int ret;

  for (cur = assigned; cur->clk.ctrl != nullptr; cur++) {
    if (cur->parent.ctrl != nullptr) {
      ret = pbl_clk_set_parent(&cur->clk, &cur->parent);
      if (ret != 0) {
        return ret;
      }
    }
  }

  for (cur = assigned; cur->clk.ctrl != nullptr; cur++) {
    if (cur->rate_hz != 0U) {
      ret = pbl_clk_set_rate(&cur->clk, (uint64_t)cur->rate_hz * 1000U);
      if (ret != 0) {
        return ret;
      }
    }
  }

  return 0;
}

void pbl_clk_notifier_register(struct pbl_clk_notifier *nb) {
  pbl_irq_lock();
  nb->next = s_notifiers;
  s_notifiers = nb;
  pbl_irq_unlock();
}

// Whether a clock is top or below it
static bool prv_below(const struct pbl_clk *clk, const struct pbl_clk *top) {
  struct pbl_clk cur = *clk;

  for (uint8_t depth = 0U; depth <= MAX_DEPTH; depth++) {
    if (prv_same(&cur, top)) {
      return true;
    }
    if (!prv_valid(&cur) || pbl_clk_get_parent(&cur, &cur) != 0) {
      return false;
    }
  }

  return false;
}

void pbl_clk_rate_changed(const struct pbl_clk *clk) {
  struct pbl_clk_notifier *nb;
  uint64_t rate_mhz;

  pbl_irq_lock();
  nb = s_notifiers;
  pbl_irq_unlock();

  for (; nb != nullptr; nb = nb->next) {
    if (prv_below(&nb->clk, clk) && pbl_clk_get_rate(&nb->clk, &rate_mhz) == 0) {
      nb->fn(nb, rate_mhz);
    }
  }
}
