/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_priv.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/watchdog.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/task_wdt/task_wdt.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

PBL_DEVICE_CLASS_DEFINE(pbl_flash_class, "flash");

#define ERASED_BYTE       0xFFU
#define BLANK_CHECK_CHUNK 128U
#define ERASE_MAX_RETRIES 3U
// Time an erase gets to progress before a read or write suspends it
#define ERASE_MIN_SLICE_MS          100U
#define ERASE_RESUME_AFTER_READ_MS  5U
#define ERASE_RESUME_AFTER_WRITE_MS 50U
// A single erase unit taking longer than this stops being covered for the task watchdog
#define ERASE_UNIT_WDT_MAX_MS 5000U
// Lets lower priority tasks run between erase units that do not wait
#define ERASE_YIELD_MS   4U
#define CD_ERASE_POLL_US 100U

static bool prv_in_range(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  return addr >= flash->base && len <= flash->geometry->size &&
         (addr - flash->base) <= (flash->geometry->size - len);
}

static bool prv_is_protected(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  const struct pbl_flash_state *st = flash->state;

  return st->protect.enabled && addr < st->protect.end && (addr + len) > st->protect.start;
}

static bool prv_ready(const struct pbl_flash *flash) {
  return flash->state->coredump || (!flash->state->stopped && pbl_device_is_ready(&flash->dev));
}

static bool prv_is_blank(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  uint8_t buf[BLANK_CHECK_CHUNK];

  while (len > 0U) {
    size_t chunk = MIN(sizeof(buf), len);
    int ret;

    ret = flash->ops->read(flash, addr, buf, chunk);
    PBL_ASSERT(ret == 0, "Flash read failed: %d", ret);

    for (size_t i = 0U; i < chunk; i++) {
      if (buf[i] != ERASED_BYTE) {
        return false;
      }
    }

    addr += chunk;
    len -= chunk;
  }

  return true;
}

static void prv_erase_pause(const struct pbl_flash *flash) {
  struct pbl_flash_state *st = flash->state;
  int ret;

  if (!st->erase.in_progress || st->erase.suspended || st->erase.done ||
      flash->ops->erase_suspend == nullptr) {
    return;
  }

  // Without a minimum slice back-to-back accesses would starve the erase
  pbl_sleep_ms(ERASE_MIN_SLICE_MS);
  pbl_task_wdt_feed_thread(st->erase.thread);

  ret = flash->ops->erase_suspend(flash);
  PBL_ASSERT(ret >= 0, "Erase suspend failed: %d", ret);
  if (ret > 0) {
    st->erase.done = true;
  } else {
    st->erase.suspended = true;
  }
}

static void prv_erase_resume(const struct pbl_flash *flash) {
  struct pbl_flash_state *st = flash->state;
  int ret;

  if (!st->erase.suspended) {
    return;
  }

  ret = flash->ops->erase_resume(flash);
  PBL_ASSERT(ret == 0, "Erase resume failed: %d", ret);
  st->erase.suspended = false;
}

static void prv_resume_timer_cb(void *arg) {
  const struct pbl_flash *flash = arg;

  pbl_mutex_lock(&flash->state->lock, PBL_FOREVER);
  prv_erase_resume(flash);
  pbl_mutex_unlock(&flash->state->lock);
}

static void prv_schedule_resume(const struct pbl_flash *flash, uint32_t delay_ms) {
  if (flash->state->erase.suspended) {
    new_timer_start(flash->state->resume_timer, delay_ms, prv_resume_timer_cb, (void *)flash, 0U);
  }
}

static uint32_t prv_erase_unit(const struct pbl_flash *flash, uint32_t addr, uint32_t end) {
  const struct pbl_flash_geometry *geo = flash->geometry;

  if (((addr - flash->base) % geo->sector_size) == 0U && (end - addr) >= geo->sector_size) {
    return geo->sector_size;
  }

  return geo->subsector_size;
}

static void prv_erase_unit_done(const struct pbl_flash *flash) {
  struct pbl_flash_state *st = flash->state;

  PBL_ANALYTICS_ADD(flash_spi_erase_bytes, st->erase.unit);
  st->erase.next += st->erase.unit;
  st->erase.retries = 0U;
}

// Advances the erase by at most one hardware operation, returning the status once it is over
// or -EINPROGRESS with the time to wait before the next step in wait_ms (0: just yield).
static int prv_erase_step_locked(const struct pbl_flash *flash, uint32_t *wait_ms) {
  struct pbl_flash_state *st = flash->state;
  uint32_t addr;
  uint32_t unit;
  int ret;

  *wait_ms = 0U;

  if (st->erase.in_progress) {
    ret = flash->ops->erase_status(flash);
    if (ret == -EBUSY || ret == -EAGAIN) {
      *wait_ms = MAX(1U, st->erase.expected_ms / 8U);
      return -EINPROGRESS;
    }

    st->erase.in_progress = false;
    st->erase.done = false;

    if (ret == 0) {
      prv_erase_unit_done(flash);
    } else if (st->erase.retries < ERASE_MAX_RETRIES) {
      st->erase.retries++;
      PBL_LOG_DBG("Erase of 0x%" PRIx32 " failed (%d), retrying", st->erase.next, ret);
    } else {
      return ret;
    }
  }

  if (st->erase.next >= st->erase.end) {
    return 0;
  }

  if (st->stopped) {
    return -ENODEV;
  }

  addr = st->erase.next;
  unit = prv_erase_unit(flash, addr, st->erase.end);
  st->erase.unit = unit;

  if (prv_is_protected(flash, addr, unit)) {
    return -EACCES;
  }

  if (prv_is_blank(flash, addr, unit)) {
    st->erase.next += unit;
    return -EINPROGRESS;
  }

  ret = flash->ops->erase(flash, addr, unit);
  if (ret < 0) {
    return ret;
  }

  if (ret == 1 || flash->ops->erase_status == nullptr) {
    prv_erase_unit_done(flash);
    return -EINPROGRESS;
  }

  st->erase.in_progress = true;
  st->erase.expected_ms = (unit == flash->geometry->sector_size)
                              ? flash->geometry->sector_erase_ms
                              : flash->geometry->subsector_erase_ms;
  *wait_ms = MAX(1U, (st->erase.expected_ms * 7U) / 8U);

  return -EINPROGRESS;
}

static bool prv_erase_step(const struct pbl_flash *flash, uint32_t *wait_ms) {
  struct pbl_flash_state *st = flash->state;
  pbl_flash_erase_cb_t cb;
  void *arg;
  int ret;

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  ret = prv_erase_step_locked(flash, wait_ms);
  if (ret == -EINPROGRESS) {
    pbl_mutex_unlock(&st->lock);
    return false;
  }

  if (ret != 0) {
    PBL_LOG_ERR("Erase of 0x%" PRIx32 " failed: %d", st->erase.next, ret);
  }

  cb = st->erase.cb;
  arg = st->erase.arg;
  pbl_mutex_unlock(&st->lock);
  pbl_sem_give(&st->erase_sem);

  // No lock held, so that the callback can start another erase
  cb(ret, arg);

  return true;
}

static void prv_poll_timer_cb(void *arg) {
  const struct pbl_flash *flash = arg;
  uint32_t wait_ms;

  if (!prv_erase_step(flash, &wait_ms)) {
    new_timer_start(flash->state->poll_timer, wait_ms, prv_poll_timer_cb, arg, 0U);
  }
}

static int prv_erase_check(const struct pbl_flash *flash, uint32_t addr, size_t *len) {
  const uint32_t subsector_size = flash->geometry->subsector_size;

  if (!prv_ready(flash)) {
    return -ENODEV;
  }

  if (((addr - flash->base) % subsector_size) != 0U) {
    return -EINVAL;
  }

  *len = ((*len + subsector_size - 1U) / subsector_size) * subsector_size;
  if (!prv_in_range(flash, addr, *len)) {
    return -EINVAL;
  }

  return 0;
}

static int prv_erase_start(const struct pbl_flash *flash, uint32_t addr, size_t len,
                           pbl_flash_erase_cb_t cb, void *arg) {
  struct pbl_flash_state *st = flash->state;

  pbl_sem_take(&st->erase_sem, PBL_FOREVER);
  pbl_mutex_lock(&st->lock, PBL_FOREVER);

  if (prv_is_protected(flash, addr, len)) {
    pbl_mutex_unlock(&st->lock);
    pbl_sem_give(&st->erase_sem);
    PBL_LOG_ERR("Erase of protected 0x%" PRIx32, addr);
    return -EACCES;
  }

  st->erase.in_progress = false;
  st->erase.suspended = false;
  st->erase.done = false;
  st->erase.retries = 0U;
  st->erase.next = addr;
  st->erase.end = addr + len;
  st->erase.thread = pbl_thread_current();
  st->erase.cb = cb;
  st->erase.arg = arg;

  pbl_mutex_unlock(&st->lock);

  return 0;
}

static int prv_erase_coredump(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  uint32_t end = addr + len;
  int ret;

  while (addr < end) {
    uint32_t unit = prv_erase_unit(flash, addr, end);

    if (prv_is_protected(flash, addr, unit)) {
      return -EACCES;
    }

    watchdog_feed();

    ret = flash->ops->erase(flash, addr, unit);
    if (ret < 0) {
      return ret;
    }

    if (ret == 0 && flash->ops->erase_status != nullptr) {
      while ((ret = flash->ops->erase_status(flash)) == -EBUSY) {
        pbl_busy_wait_us(CD_ERASE_POLL_US);
      }
      if (ret != 0) {
        return ret;
      }
    }

    addr += unit;
  }

  watchdog_feed();

  return 0;
}

static void prv_blocking_erase_cb(int status, void *arg) {
  *(int *)arg = status;
}

int pbl_flash_dev_init(const struct pbl_device *dev) {
  const struct pbl_flash *flash = container_of(dev, const struct pbl_flash, dev);
  struct pbl_flash_state *st = flash->state;
  int ret;

  st->poll_timer = new_timer_create();
  st->resume_timer = new_timer_create();

  if (flash->ops->init == nullptr) {
    return 0;
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  ret = flash->ops->init(flash);
  pbl_mutex_unlock(&st->lock);

  return ret;
}

int pbl_flash_coredump_init(const struct pbl_flash *flash) {
  flash->state->coredump = true;

  if (flash->ops->init == nullptr) {
    return 0;
  }

  return flash->ops->init(flash);
}

void pbl_flash_stop(const struct pbl_flash *flash) {
  struct pbl_flash_state *st = flash->state;

  if (!pbl_device_is_ready(&flash->dev) || st->stopped) {
    return;
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  if (st->erase.in_progress) {
    new_timer_stop(st->resume_timer);
    prv_erase_resume(flash);
  }
  pbl_mutex_unlock(&st->lock);

  while (__atomic_load_n(&st->erase.in_progress, __ATOMIC_SEQ_CST)) {
    pbl_sleep_ms(ERASE_YIELD_MS);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  if (flash->ops->stop != nullptr) {
    flash->ops->stop(flash);
  }
  st->stopped = true;
  pbl_mutex_unlock(&st->lock);
}

// Serializes an access that is neither a read, a write nor an erase of the array
int pbl_flash__access_begin(const struct pbl_flash *flash, uint32_t resume_ms) {
  struct pbl_flash_state *st = flash->state;

  if (!prv_ready(flash)) {
    return -ENODEV;
  }

  if (st->coredump) {
    return 0;
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  prv_erase_pause(flash);
  prv_schedule_resume(flash, resume_ms);

  return 0;
}

void pbl_flash__access_end(const struct pbl_flash *flash) {
  if (!flash->state->coredump) {
    pbl_mutex_unlock(&flash->state->lock);
  }
}

int pbl_flash_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  struct pbl_flash_state *st = flash->state;
  int ret;

  if (!prv_ready(flash)) {
    return -ENODEV;
  }

  if (!prv_in_range(flash, addr, len)) {
    return -EINVAL;
  }

  if (len == 0U) {
    return 0;
  }

  if (st->coredump) {
    return flash->ops->read(flash, addr, buf, len);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  prv_erase_pause(flash);
  prv_schedule_resume(flash, ERASE_RESUME_AFTER_READ_MS);
  ret = flash->ops->read(flash, addr, buf, len);
  pbl_mutex_unlock(&st->lock);

  PBL_ASSERT(ret == 0, "Flash read failed: %d", ret);

  return 0;
}

int pbl_flash_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  struct pbl_flash_state *st = flash->state;
  int ret;

  if (!prv_ready(flash)) {
    return -ENODEV;
  }

  if (!prv_in_range(flash, addr, len)) {
    return -EINVAL;
  }

  if (len == 0U) {
    return 0;
  }

  if (st->coredump) {
    if (prv_is_protected(flash, addr, len)) {
      return -EACCES;
    }

    return flash->ops->write(flash, addr, buf, len);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);

  if (prv_is_protected(flash, addr, len)) {
    pbl_mutex_unlock(&st->lock);
    PBL_LOG_ERR("Write to protected 0x%" PRIx32, addr);
    return -EACCES;
  }

  prv_erase_pause(flash);
  prv_schedule_resume(flash, ERASE_RESUME_AFTER_WRITE_MS);
  PBL_ANALYTICS_ADD(flash_spi_write_bytes, len);
  ret = flash->ops->write(flash, addr, buf, len);
  pbl_mutex_unlock(&st->lock);

  PBL_ASSERT(ret == 0, "Flash write failed: %d", ret);

  return 0;
}

int pbl_flash_erase(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  struct pbl_flash_state *st = flash->state;
  int status = 0;
  uint32_t wait_ms;
  uint32_t resume_ms;
  uint32_t unit_waited_ms = 0U;
  uint32_t last_next = addr;
  uint32_t steps = 0U;
  int ret;

  ret = prv_erase_check(flash, addr, &len);
  if (ret != 0) {
    return ret;
  }

  if (len == 0U) {
    return 0;
  }

  if (st->coredump) {
    return prv_erase_coredump(flash, addr, len);
  }

  ret = prv_erase_start(flash, addr, len, prv_blocking_erase_cb, &status);
  if (ret != 0) {
    return ret;
  }

  while (!prv_erase_step(flash, &wait_ms)) {
    if (wait_ms > 0U) {
      pbl_sleep_ms(wait_ms);
    } else if ((++steps % 2U) == 0U) {
      pbl_sleep_ms(ERASE_YIELD_MS);
    }

    // The resume timer may be starved by this task
    if (new_timer_scheduled(st->resume_timer, &resume_ms) && resume_ms == 0U) {
      prv_resume_timer_cb((void *)flash);
    }

    if (st->erase.next != last_next) {
      last_next = st->erase.next;
      unit_waited_ms = 0U;
    } else {
      unit_waited_ms += wait_ms;
    }

    // Worn parts can take seconds per erase: only give up covering a unit that never completes
    if (unit_waited_ms < ERASE_UNIT_WDT_MAX_MS) {
      pbl_task_wdt_feed_self();
    }
  }

  PBL_ASSERT(status == 0 || status == -EACCES || status == -ENODEV, "Flash erase failed: %d",
             status);

  return status;
}

int pbl_flash_erase_async(const struct pbl_flash *flash, uint32_t addr, size_t len,
                          pbl_flash_erase_cb_t cb, void *arg) {
  int ret;

  if (flash->state->coredump) {
    return -EPERM;
  }

  ret = prv_erase_check(flash, addr, &len);
  if (ret != 0) {
    return ret;
  }

  if (len == 0U) {
    cb(0, arg);
    return 0;
  }

  ret = prv_erase_start(flash, addr, len, cb, arg);
  if (ret != 0) {
    return ret;
  }

  prv_poll_timer_cb((void *)flash);

  return 0;
}

bool pbl_flash_is_erased(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  struct pbl_flash_state *st = flash->state;
  bool blank;

  if (!prv_ready(flash) || !prv_in_range(flash, addr, len)) {
    return false;
  }

  if (st->coredump) {
    return prv_is_blank(flash, addr, len);
  }

  pbl_mutex_lock(&st->lock, PBL_FOREVER);
  prv_erase_pause(flash);
  prv_schedule_resume(flash, ERASE_RESUME_AFTER_READ_MS);
  blank = prv_is_blank(flash, addr, len);
  pbl_mutex_unlock(&st->lock);

  return blank;
}

int pbl_flash_protect(const struct pbl_flash *flash, uint32_t addr, size_t len) {
  struct pbl_flash_state *st = flash->state;

  if (!prv_in_range(flash, addr, len)) {
    return -EINVAL;
  }

  if (!st->coredump) {
    pbl_mutex_lock(&st->lock, PBL_FOREVER);
  }

  st->protect.start = addr;
  st->protect.end = addr + len;
  st->protect.enabled = true;

  if (!st->coredump) {
    pbl_mutex_unlock(&st->lock);
  }

  return 0;
}

int pbl_flash_unprotect(const struct pbl_flash *flash) {
  struct pbl_flash_state *st = flash->state;

  if (!st->coredump) {
    pbl_mutex_lock(&st->lock, PBL_FOREVER);
  }

  st->protect.enabled = false;

  if (!st->coredump) {
    pbl_mutex_unlock(&st->lock);
  }

  return 0;
}

// An erase in progress keeps the flash and its controller powered
static bool prv_can_power_down(const struct pbl_flash *flash) {
  return pbl_device_is_ready(&flash->dev) && !flash->state->erase.in_progress;
}

void pbl_flash_power_down(const struct pbl_flash *flash, pbl_flash_pm_fn_t fn, void *arg) {
  if (flash->ops->power_down == nullptr || !prv_can_power_down(flash)) {
    fn(arg);
    return;
  }

  flash->ops->power_down(flash, fn, arg);
}
