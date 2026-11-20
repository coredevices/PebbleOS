/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/intc.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/bits.h>

PBL_DEVICE_CLASS_DEFINE(pbl_intc_class, "intc");

static bool prv_type_valid(uint32_t type) {
  return (type == PBL_IRQ_TYPE_EDGE_RISING) || (type == PBL_IRQ_TYPE_EDGE_FALLING) ||
         (type == PBL_IRQ_TYPE_EDGE_BOTH) || (type == PBL_IRQ_TYPE_LEVEL_HIGH) ||
         (type == PBL_IRQ_TYPE_LEVEL_LOW);
}

static bool prv_enabled(const struct pbl_intc *intc, uint16_t hwirq) {
  return (intc->enabled[hwirq / 32U] & PBL_BIT(hwirq % 32U)) != 0U;
}

static void prv_mark_enabled(const struct pbl_intc *intc, uint16_t hwirq, bool enabled) {
  if (enabled) {
    intc->enabled[hwirq / 32U] |= PBL_BIT(hwirq % 32U);
  } else {
    intc->enabled[hwirq / 32U] &= ~PBL_BIT(hwirq % 32U);
  }
}

static struct pbl_intc_line *prv_requested_line(const struct pbl_irq_spec *spec) {
  const struct pbl_intc *intc = spec->intc;

  if (spec->hwirq >= intc->num_irqs || intc->lines[spec->hwirq].handler == nullptr) {
    return nullptr;
  }

  return &intc->lines[spec->hwirq];
}

int pbl_intc_request(const struct pbl_irq_spec *spec, pbl_intc_handler_t handler, void *arg) {
  const struct pbl_intc *intc = spec->intc;
  struct pbl_intc_line *line;
  int ret;

  PBL_ASSERTN(pbl_device_is_ready(&intc->dev));

  if (spec->hwirq >= intc->num_irqs || !prv_type_valid(spec->type) || handler == nullptr) {
    return -EINVAL;
  }

  line = &intc->lines[spec->hwirq];

  pbl_irq_lock();

  if (line->handler != nullptr) {
    pbl_irq_unlock();
    return -EBUSY;
  }

  ret = intc->ops->configure(intc, spec->hwirq, spec->type);
  if (ret != 0) {
    pbl_irq_unlock();
    return ret;
  }

  line->handler = handler;
  line->arg = arg;
  prv_mark_enabled(intc, spec->hwirq, false);

  pbl_irq_unlock();

  return 0;
}

int pbl_intc_free(const struct pbl_irq_spec *spec) {
  struct pbl_intc_line *line;
  int ret = 0;

  pbl_irq_lock();

  line = prv_requested_line(spec);
  if (line == nullptr) {
    pbl_irq_unlock();
    return -EINVAL;
  }

  if (prv_enabled(spec->intc, spec->hwirq)) {
    ret = spec->intc->ops->disable(spec->intc, spec->hwirq);
    if (ret != 0) {
      pbl_irq_unlock();
      return ret;
    }
  }

  line->handler = nullptr;
  line->arg = nullptr;
  prv_mark_enabled(spec->intc, spec->hwirq, false);

  pbl_irq_unlock();

  return 0;
}

static int prv_set_enabled(const struct pbl_irq_spec *spec, bool enable) {
  const struct pbl_intc *intc = spec->intc;
  int ret = 0;

  pbl_irq_lock();

  if (prv_requested_line(spec) == nullptr) {
    pbl_irq_unlock();
    return -EINVAL;
  }

  if (prv_enabled(intc, spec->hwirq) != enable) {
    ret = enable ? intc->ops->enable(intc, spec->hwirq) : intc->ops->disable(intc, spec->hwirq);
    if (ret == 0) {
      prv_mark_enabled(intc, spec->hwirq, enable);
    }
  }

  pbl_irq_unlock();

  return ret;
}

int pbl_intc_enable(const struct pbl_irq_spec *spec) {
  return prv_set_enabled(spec, true);
}

int pbl_intc_disable(const struct pbl_irq_spec *spec) {
  return prv_set_enabled(spec, false);
}

int pbl_intc_set_wake(const struct pbl_irq_spec *spec, bool on) {
  const struct pbl_intc *intc = spec->intc;
  int ret;

  if (intc->ops->set_wake == nullptr) {
    return -ENOTSUP;
  }

  pbl_irq_lock();

  if (prv_requested_line(spec) == nullptr) {
    pbl_irq_unlock();
    return -EINVAL;
  }

  ret = intc->ops->set_wake(intc, spec->hwirq, on);

  pbl_irq_unlock();

  return ret;
}

void pbl_intc_dispatch(const struct pbl_intc *intc, uint32_t hwirq) {
  const struct pbl_intc_line *line;

  if (hwirq >= intc->num_irqs) {
    return;
  }

  line = &intc->lines[hwirq];
  if (line->handler == nullptr) {
    pbl_irq_lock();
    (void)intc->ops->disable(intc, hwirq);
    pbl_irq_unlock();
    return;
  }

  line->handler(line->arg);
}
