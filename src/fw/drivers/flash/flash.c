/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>

#include <pbl/drivers/task_watchdog.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/logging/logging.h>
#include "kernel/pebble_tasks.h"
#include "kernel/util/delay.h"
#include "kernel/util/sleep.h"
#include "pbl/services/analytics/analytics.h"
#include "pbl/util/math.h"
#include "system/passert.h"

PBL_LOG_MODULE_DEFINE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

#define ERASE_MAX_RETRIES 3
// Minimum time an erase gets to progress before a read or write suspends it.
#define ERASE_MIN_SLICE_MS 100
#define ERASE_RESUME_AFTER_READ_MS 5
#define ERASE_RESUME_AFTER_WRITE_MS 50
// Stop feeding the watchdog once a single erase unit has taken this long.
#define ERASE_UNIT_WATCHDOG_MS 5000
// Let lower priority tasks run in between synchronous erase units.
#define ERASE_YIELD_MS 4

static bool prv_in_range(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  return addr >= dev->base && len <= dev->geometry->size &&
         addr - dev->base <= dev->geometry->size - len;
}

static bool prv_is_protected(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  const struct pbl_flash_device_state *st = dev->state;

  return st->protect.enabled && addr < st->protect.end && addr + len > st->protect.start;
}

// Lock held.
static bool prv_is_blank(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  uint32_t buf[32];

  while (len > 0) {
    size_t chunk = MIN(sizeof(buf), len);
    int ret = dev->ops->read(dev, addr, buf, chunk);
    PBL_ASSERT(ret == 0, "Flash read failed: %d", ret);
    for (size_t i = 0; i < chunk; i++) {
      if (((const uint8_t *)buf)[i] != 0xFF) {
        return false;
      }
    }
    addr += chunk;
    len -= chunk;
  }

  return true;
}

// Lock held.
static void prv_erase_pause(const struct pbl_flash_device *dev) {
  struct pbl_flash_device_state *st = dev->state;

  if (!st->erase.in_progress || st->erase.suspended || st->erase.done ||
      dev->ops->erase_suspend == NULL) {
    return;
  }

  psleep(ERASE_MIN_SLICE_MS);
  int ret = dev->ops->erase_suspend(dev);
  PBL_ASSERT(ret >= 0, "Erase suspend failure: %d", ret);
  if (ret > 0) {
    // Completed on its own; the next poll collects the result.
    st->erase.done = true;
  } else {
    st->erase.suspended = true;
  }
}

// Lock held.
static void prv_erase_resume(const struct pbl_flash_device *dev) {
  struct pbl_flash_device_state *st = dev->state;

  if (!st->erase.suspended) {
    return;
  }

  int ret = dev->ops->erase_resume(dev);
  PBL_ASSERT(ret == 0, "Erase resume failure: %d", ret);
  st->erase.suspended = false;
}

static void prv_resume_timer_cb(void *ctx) {
  const struct pbl_flash_device *dev = ctx;

  pbl_mutex_lock(&dev->state->lock, PBL_FOREVER);
  prv_erase_resume(dev);
  pbl_mutex_unlock(&dev->state->lock);
}

static void prv_schedule_resume(const struct pbl_flash_device *dev, uint32_t delay_ms) {
  if (dev->state->erase.suspended) {
    new_timer_start(dev->state->resume_timer, delay_ms, prv_resume_timer_cb, (void *)dev, 0);
  }
}

static uint32_t prv_erase_unit(const struct pbl_flash_device *dev, uint32_t addr, uint32_t end) {
  if ((addr & (dev->geometry->sector_size - 1)) == 0 && addr + dev->geometry->sector_size <= end) {
    return dev->geometry->sector_size;
  }
  return dev->geometry->subsector_size;
}

// Lock held.
static void prv_erase_unit_done(const struct pbl_flash_device *dev) {
  struct pbl_flash_device_state *st = dev->state;

  PBL_ANALYTICS_ADD(flash_spi_erase_bytes, st->erase.unit);
  st->erase.next += st->erase.unit;
  st->erase.retries = 0;
}

//! Advances the erase job by at most one hardware operation.
//! @return true when the job has finished (callback invoked), otherwise
//!         @p wait_ms holds the delay before the next call (0: just yield).
static bool prv_erase_step(const struct pbl_flash_device *dev, uint32_t *wait_ms) {
  struct pbl_flash_device_state *st = dev->state;
  bool finished = false;
  int status = 0;

  *wait_ms = 0;
  pbl_mutex_lock(&st->lock, PBL_FOREVER);

  if (st->erase.in_progress) {
    int ret = dev->ops->erase_status(dev);
    if (ret == -EBUSY || ret == -EAGAIN) {
      *wait_ms = MAX(1, st->erase.expected_ms / 8);
      goto out;
    }
    st->erase.in_progress = false;
    st->erase.done = false;
    if (ret == 0) {
      prv_erase_unit_done(dev);
    } else if (st->erase.retries < ERASE_MAX_RETRIES) {
      st->erase.retries++;
      PBL_LOG_DBG("Erase of 0x%" PRIx32 " failed (%d), retrying", st->erase.next, ret);
    } else {
      status = ret;
      finished = true;
      goto out;
    }
  }

  if (st->erase.next >= st->erase.end) {
    finished = true;
    goto out;
  }

  uint32_t addr = st->erase.next;
  uint32_t unit = prv_erase_unit(dev, addr, st->erase.end);
  st->erase.unit = unit;

  if (prv_is_protected(dev, addr, unit)) {
    status = -EACCES;
    finished = true;
    goto out;
  }

  if (prv_is_blank(dev, addr, unit)) {
    st->erase.next += unit;
    goto out;
  }

  int ret = dev->ops->erase_begin(dev, addr, unit);
  if (ret != 0) {
    status = ret;
    finished = true;
    goto out;
  }

  if (dev->ops->erase_status != NULL) {
    st->erase.in_progress = true;
    st->erase.expected_ms = (unit == dev->geometry->sector_size)
                                ? dev->geometry->sector_erase_ms
                                : dev->geometry->subsector_erase_ms;
    *wait_ms = MAX(1, st->erase.expected_ms * 7 / 8);
  } else {
    prv_erase_unit_done(dev);
  }

out:
  if (finished) {
    pbl_flash_erase_cb_t cb = st->erase.cb;
    void *ctx = st->erase.ctx;

    pbl_mutex_unlock(&st->lock);
    pbl_sem_give(&st->erase_sem);
    // No locks held so that the callback can start another erase.
    cb(ctx, status);
  } else {
    pbl_mutex_unlock(&st->lock);
  }

  return finished;
}

static void prv_poll_timer_cb(void *ctx) {
  const struct pbl_flash_device *dev = ctx;
  uint32_t wait_ms;

  if (!prv_erase_step(dev, &wait_ms)) {
    new_timer_start(dev->state->poll_timer, wait_ms, prv_poll_timer_cb, ctx, 0);
  }
}

static size_t prv_erase_len(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  PBL_ASSERTN((addr & (dev->geometry->subsector_size - 1)) == 0);
  len = (len + dev->geometry->subsector_size - 1) & ~(size_t)(dev->geometry->subsector_size - 1);
  PBL_ASSERTN(prv_in_range(dev, addr, len));
  return len;
}

static void prv_erase_start(const struct pbl_flash_device *dev, uint32_t addr, size_t len,
                            pbl_flash_erase_cb_t cb, void *ctx) {
  struct pbl_flash_device_state *st = dev->state;

  pbl_sem_take(&st->erase_sem, PBL_FOREVER);
  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  st->erase.in_progress = false;
  st->erase.suspended = false;
  st->erase.done = false;
  st->erase.next = addr;
  st->erase.end = addr + len;
  st->erase.retries = 0;
  st->erase.cb = cb;
  st->erase.ctx = ctx;
  pbl_mutex_unlock(&st->lock);
}

static void prv_blocking_erase_cb(void *ctx, int status) {
  *(int *)ctx = status;
}

static int prv_erase_coredump(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  while (len > 0) {
    uint32_t unit = prv_erase_unit(dev, addr, addr + len);

    if (prv_is_protected(dev, addr, unit)) {
      return -EACCES;
    }
    watchdog_feed();
    int ret = dev->ops->erase_begin(dev, addr, unit);
    if (ret != 0) {
      return ret;
    }
    if (dev->ops->erase_status != NULL) {
      while ((ret = dev->ops->erase_status(dev)) == -EBUSY) {
        delay_us(100);
      }
      if (ret != 0) {
        return ret;
      }
    }
    addr += unit;
    len -= unit;
  }
  watchdog_feed();

  return 0;
}

int pbl_flash_init(const struct pbl_flash_device *dev) {
  struct pbl_flash_device_state *st = dev->state;

  pbl_mutex_init(&st->lock);
  pbl_sem_init(&st->erase_sem, 1, 1);
  st->poll_timer = new_timer_create();
  st->resume_timer = new_timer_create();

  int ret = dev->ops->init(dev);
  PBL_ASSERT(ret == 0, "Flash init failed: %d", ret);
  st->initialized = true;

  return ret;
}

int pbl_flash_coredump_init(const struct pbl_flash_device *dev) {
  dev->state->coredump = true;
  return dev->ops->init(dev);
}

void pbl_flash_stop(const struct pbl_flash_device *dev) {
  struct pbl_flash_device_state *st = dev->state;

  if (!st->initialized) {
    return;
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  if (st->erase.in_progress) {
    new_timer_stop(st->resume_timer);
    prv_erase_resume(dev);
  }
  pbl_mutex_unlock(&st->lock);

  while (__atomic_load_n(&st->erase.in_progress, __ATOMIC_SEQ_CST)) {
    psleep(10);
  }
}

int pbl_flash_read(const struct pbl_flash_device *dev, uint32_t addr, void *buf, size_t len) {
  struct pbl_flash_device_state *st = dev->state;

  PBL_ASSERTN(prv_in_range(dev, addr, len));
  if (len == 0) {
    return 0;
  }

  if (st->coredump) {
    return dev->ops->read(dev, addr, buf, len);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  prv_erase_pause(dev);
  prv_schedule_resume(dev, ERASE_RESUME_AFTER_READ_MS);
  int ret = dev->ops->read(dev, addr, buf, len);
  PBL_ASSERT(ret == 0, "Flash read failed: %d", ret);
  pbl_mutex_unlock(&st->lock);

  return 0;
}

int pbl_flash_write(const struct pbl_flash_device *dev, uint32_t addr, const void *buf,
                    size_t len) {
  struct pbl_flash_device_state *st = dev->state;

  PBL_ASSERTN(prv_in_range(dev, addr, len));
  if (len == 0) {
    return 0;
  }

  if (st->coredump) {
    if (prv_is_protected(dev, addr, len)) {
      return -EACCES;
    }
    return dev->ops->write(dev, addr, buf, len);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  PBL_ASSERT(!prv_is_protected(dev, addr, len), "Write to protected flash at 0x%" PRIx32, addr);
  prv_erase_pause(dev);
  prv_schedule_resume(dev, ERASE_RESUME_AFTER_WRITE_MS);
  PBL_ANALYTICS_ADD(flash_spi_write_bytes, len);
  int ret = dev->ops->write(dev, addr, buf, len);
  PBL_ASSERT(ret == 0, "Flash write failed: %d", ret);
  pbl_mutex_unlock(&st->lock);

  return 0;
}

int pbl_flash_erase(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  struct pbl_flash_device_state *st = dev->state;
  int status = 0;
  uint32_t wait_ms;
  uint32_t unit_waited_ms = 0;
  uint32_t last_next = addr;
  unsigned int yields = 0;

  len = prv_erase_len(dev, addr, len);
  if (len == 0) {
    return 0;
  }

  if (st->coredump) {
    return prv_erase_coredump(dev, addr, len);
  }

  prv_erase_start(dev, addr, len, prv_blocking_erase_cb, &status);

  while (!prv_erase_step(dev, &wait_ms)) {
    if (wait_ms > 0) {
      psleep(wait_ms);
    } else if ((++yields % 2) == 0) {
      psleep(ERASE_YIELD_MS);
    }

    // The resume timer callback may be starved by this task. See PBL-25741.
    uint32_t resume_remaining_ms;
    if (new_timer_scheduled(st->resume_timer, &resume_remaining_ms) && resume_remaining_ms == 0) {
      prv_resume_timer_cb((void *)dev);
    }

    uint32_t next = __atomic_load_n(&st->erase.next, __ATOMIC_RELAXED);
    if (next != last_next) {
      last_next = next;
      unit_waited_ms = 0;
    } else {
      unit_waited_ms += wait_ms;
    }

    // Erases can take seconds on worn parts; stop appeasing the watchdog only
    // if a single unit never completes.
    if (unit_waited_ms < ERASE_UNIT_WATCHDOG_MS) {
#ifdef CONFIG_IS_BIGBOARD
      task_watchdog_bit_set_all();
#else
      task_watchdog_bit_set(pebble_task_get_current());
#endif
    }
  }

  PBL_ASSERT(status == 0, "Flash erase failure: %d", status);

  return 0;
}

int pbl_flash_erase_async(const struct pbl_flash_device *dev, uint32_t addr, size_t len,
                          pbl_flash_erase_cb_t cb, void *ctx) {
  PBL_ASSERTN(!dev->state->coredump);

  len = prv_erase_len(dev, addr, len);
  if (len == 0) {
    cb(ctx, 0);
    return 0;
  }

  prv_erase_start(dev, addr, len, cb, ctx);
  prv_poll_timer_cb((void *)dev);

  return 0;
}

bool pbl_flash_is_erased(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  struct pbl_flash_device_state *st = dev->state;

  PBL_ASSERTN(prv_in_range(dev, addr, len));

  if (st->coredump) {
    return prv_is_blank(dev, addr, len);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  prv_erase_pause(dev);
  prv_schedule_resume(dev, ERASE_RESUME_AFTER_READ_MS);
  bool blank = prv_is_blank(dev, addr, len);
  pbl_mutex_unlock(&st->lock);

  return blank;
}

int pbl_flash_protect(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  struct pbl_flash_device_state *st = dev->state;

  PBL_ASSERTN(prv_in_range(dev, addr, len));

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  st->protect.start = addr;
  st->protect.end = addr + len;
  st->protect.enabled = true;
  pbl_mutex_unlock(&st->lock);

  return 0;
}

int pbl_flash_unprotect(const struct pbl_flash_device *dev) {
  struct pbl_flash_device_state *st = dev->state;

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  st->protect.enabled = false;
  pbl_mutex_unlock(&st->lock);

  return 0;
}

void pbl_flash_power_down(const struct pbl_flash_device *dev) {
  if (dev->ops->power_down != NULL) {
    dev->ops->power_down(dev);
  }
}

void pbl_flash_power_up(const struct pbl_flash_device *dev) {
  if (dev->ops->power_up != NULL) {
    dev->ops->power_up(dev);
  }
}

#define SEC_REG_OP(dev, op, ...)                      \
  do {                                                \
    struct pbl_flash_device_state *st = (dev)->state; \
    int ret;                                          \
    if ((dev)->ops->op == NULL) {                     \
      return -ENOTSUP;                                \
    }                                                 \
    if (st->coredump) {                               \
      return (dev)->ops->op(dev, __VA_ARGS__);        \
    }                                                 \
    pbl_mutex_lock(&st->lock, PBL_FOREVER);           \
    ret = (dev)->ops->op(dev, __VA_ARGS__);           \
    pbl_mutex_unlock(&st->lock);                      \
    return ret;                                       \
  } while (0)

int pbl_flash_sec_reg_read(const struct pbl_flash_device *dev, uint32_t addr, uint8_t *val) {
  SEC_REG_OP(dev, sec_reg_read, addr, val);
}

int pbl_flash_sec_reg_write(const struct pbl_flash_device *dev, uint32_t addr, uint8_t val) {
  SEC_REG_OP(dev, sec_reg_write, addr, val);
}

int pbl_flash_sec_reg_erase(const struct pbl_flash_device *dev, uint32_t addr) {
  SEC_REG_OP(dev, sec_reg_erase, addr);
}

int pbl_flash_sec_reg_is_locked(const struct pbl_flash_device *dev, uint32_t addr, bool *locked) {
  SEC_REG_OP(dev, sec_reg_is_locked, addr, locked);
}

#ifdef CONFIG_RECOVERY_FW
int pbl_flash_sec_reg_lock(const struct pbl_flash_device *dev, uint32_t addr) {
  SEC_REG_OP(dev, sec_reg_lock, addr);
}
#endif
