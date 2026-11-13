/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/analytics/analytics.h>
#include <pbl/drivers/ram.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/cache.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_ram_class, "ram");

int pbl_ram_dev_init(const struct pbl_device *dev) {
  const struct pbl_ram *ram = container_of(dev, const struct pbl_ram, dev);

  return ram->ops->init(ram);
}

void pbl_ram_get_region(const struct pbl_ram *ram, uintptr_t *base, size_t *size) {
  *base = ram->state->base;
  *size = pbl_device_is_ready(&ram->dev) ? ram->state->size : 0U;
}

int pbl_ram_get(const struct pbl_ram *ram) {
  struct pbl_ram_state *st = ram->state;
  int ret = 0;

  if (!pbl_device_is_ready(&ram->dev)) {
    return -ENODEV;
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  if (!st->on) {
    st->busy = true;
    ret = ram->ops->power_on(ram);
    if (ret == 0) {
      st->on = true;
      st->power_cycles++;
    }
    st->busy = false;
  }
  if ((ret == 0) && (st->users++ == 0U)) {
    PBL_ANALYTICS_TIMER_START(ram_used_ms);
  }
  pbl_mutex_unlock(&st->lock);

  return ret;
}

void pbl_ram_put(const struct pbl_ram *ram) {
  struct pbl_ram_state *st = ram->state;

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  PBL_ASSERTN(st->users > 0U);
  if (--st->users == 0U) {
    PBL_ANALYTICS_TIMER_STOP(ram_used_ms);
  }
  pbl_mutex_unlock(&st->lock);
}

// The idle thread runs these with interrupts disabled and every other thread blocked. If one is
// blocked while powering the memory on, it is left as it is.
void pbl_ram_suspend(const struct pbl_ram *ram) {
  struct pbl_ram_state *st = ram->state;

  if (!st->on || st->busy) {
    return;
  }

#ifdef CONFIG_CPU_HAS_DCACHE
  // No write-back may reach it while it is off or retained
  pbl_dcache_flush_all();
#endif

  if (st->users == 0U) {
    ram->ops->power_off(ram);
    st->on = false;
    return;
  }

  if (ram->ops->retain != nullptr) {
    ram->ops->retain(ram);
    st->retained = true;
  }
}

void pbl_ram_resume(const struct pbl_ram *ram) {
  struct pbl_ram_state *st = ram->state;

  if (st->retained) {
    ram->ops->resume(ram);
    st->retained = false;
  }
}

void pbl_ram_quiesce(const struct pbl_ram *ram) {
  if (ram->state->on && (ram->ops->quiesce != nullptr)) {
    ram->ops->quiesce(ram);
  }
}
