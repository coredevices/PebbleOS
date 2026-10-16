/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/drivers/als.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

#include <system/passert.h>

PBL_LOG_MODULE_DEFINE(driver_als, CONFIG_DRIVER_AMBIENT_LOG_LEVEL);

static void prv_lock(const struct pbl_als *als) {
  pbl_mutex_lock(&als->state->lock, PBL_FOREVER);
}

static void prv_unlock(const struct pbl_als *als) {
  pbl_mutex_unlock(&als->state->lock);
}

static void prv_apply_state_locked(const struct pbl_als *als) {
  const struct pbl_als_state *state = als->state;
  const bool active = state->prime_count > 0U;
  const bool sampling = active && (state->suspend_count == 0U);
  int ret;

  if (als->ops->set_state == nullptr) {
    return;
  }

  ret = als->ops->set_state(als, active, sampling);
  if (ret != 0) {
    PBL_LOG_ERR("Could not apply sampling state (%d)", ret);
  }
}

int pbl_als_dev_init(const struct pbl_device *dev) {
  const struct pbl_als *als = container_of(dev, const struct pbl_als, dev);
  int ret;

  prv_lock(als);
  ret = als->ops->init(als);
  prv_unlock(als);

  return ret;
}

int pbl_als_read(const struct pbl_als *als, uint32_t *level) {
  int ret;

  if (!pbl_device_is_ready(&als->dev)) {
    return -ENODEV;
  }

  prv_lock(als);
  ret = als->ops->read(als, level);
  prv_unlock(als);

  return ret;
}

void pbl_als_prime(const struct pbl_als *als) {
  if (!pbl_device_is_ready(&als->dev)) {
    return;
  }

  prv_lock(als);
  als->state->prime_count++;
  prv_apply_state_locked(als);
  prv_unlock(als);
}

void pbl_als_release(const struct pbl_als *als) {
  if (!pbl_device_is_ready(&als->dev)) {
    return;
  }

  prv_lock(als);
  PBL_ASSERTN(als->state->prime_count > 0U);
  als->state->prime_count--;
  prv_apply_state_locked(als);
  prv_unlock(als);
}

void pbl_als_suspend(const struct pbl_als *als) {
  if (!pbl_device_is_ready(&als->dev)) {
    return;
  }

  prv_lock(als);
  als->state->suspend_count++;
  prv_apply_state_locked(als);
  prv_unlock(als);
}

void pbl_als_resume(const struct pbl_als *als) {
  if (!pbl_device_is_ready(&als->dev)) {
    return;
  }

  prv_lock(als);
  PBL_ASSERTN(als->state->suspend_count > 0U);
  als->state->suspend_count--;
  prv_apply_state_locked(als);
  prv_unlock(als);
}

bool pbl_als_has_lux(const struct pbl_als *als) {
  return als->scale != nullptr && als->scale->den != 0U;
}

uint32_t pbl_als_to_lux(const struct pbl_als *als, uint32_t level) {
  const struct pbl_als_scale *scale = als->scale;

  // Uncalibrated: keep raw counts so thresholds tuned in counts behave the same
  if (!pbl_als_has_lux(als)) {
    return level;
  }

  if (level <= scale->offset) {
    return 0U;
  }

  return (uint32_t)(((uint64_t)(level - scale->offset) * scale->num) / scale->den);
}
