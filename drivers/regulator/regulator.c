/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/regulator.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

PBL_LOG_MODULE_DEFINE(driver_regulator, CONFIG_DRIVER_REGULATOR_LOG_LEVEL);
PBL_DEVICE_CLASS_DEFINE(pbl_regulator_class, "regulator");

static int prv_switch_on(const struct pbl_regulator *reg) {
  int ret;

  ret = reg->ops->enable(reg);
  if (ret != 0) {
    return ret;
  }

  if (reg->enable_time_us >= PBL_USEC_PER_MSEC) {
    pbl_sleep_us(reg->enable_time_us);
  } else if (reg->enable_time_us != 0U) {
    pbl_busy_wait_us(reg->enable_time_us);
  }

  return 0;
}

int pbl_regulator_dev_init(const struct pbl_device *dev) {
  const struct pbl_regulator *reg = PBL_CONTAINER_OF(dev, const struct pbl_regulator, dev);
  int ret;

  pbl_mutex_init(&reg->state->mutex);
  reg->state->use_count = 0;

  if (reg->ops->init != nullptr) {
    ret = reg->ops->init(reg);
    if (ret != 0) {
      return ret;
    }
  }

  return reg->always_on ? prv_switch_on(reg) : reg->ops->disable(reg);
}

static int prv_enable_locked(const struct pbl_regulator *reg) {
  int ret;

  if (reg->state->use_count == 0) {
    ret = prv_switch_on(reg);
    if (ret != 0) {
      return ret;
    }
  }

  reg->state->use_count++;

  return 0;
}

int pbl_regulator_enable(const struct pbl_regulator *reg) {
  int ret;

  PBL_ASSERTN(pbl_device_is_ready(&reg->dev));

  if (reg->always_on) {
    return 0;
  }

  pbl_mutex_lock(&reg->state->mutex, PBL_FOREVER);
  ret = prv_enable_locked(reg);
  pbl_mutex_unlock(&reg->state->mutex);

  return ret;
}

static int prv_disable_locked(const struct pbl_regulator *reg) {
  int ret;

  PBL_ASSERTN(reg->state->use_count > 0);

  if (reg->state->use_count == 1) {
    ret = reg->ops->disable(reg);
    if (ret != 0) {
      return ret;
    }
  }

  reg->state->use_count--;

  return 0;
}

int pbl_regulator_disable(const struct pbl_regulator *reg) {
  int ret;

  if (reg->always_on) {
    return 0;
  }

  pbl_mutex_lock(&reg->state->mutex, PBL_FOREVER);
  ret = prv_disable_locked(reg);
  pbl_mutex_unlock(&reg->state->mutex);

  return ret;
}

int pbl_regulator_power_cycle(const struct pbl_regulator *reg, uint32_t off_ms) {
  int ret;

  PBL_ASSERTN(pbl_device_is_ready(&reg->dev));

  pbl_mutex_lock(&reg->state->mutex, PBL_FOREVER);
  if (reg->state->use_count > 0) {
    ret = -EBUSY;
  } else {
    ret = reg->ops->disable(reg);
    if (ret == 0) {
      pbl_sleep_ms(off_ms);
      ret = prv_switch_on(reg);
    }
  }
  pbl_mutex_unlock(&reg->state->mutex);

  return ret;
}

bool pbl_regulator_is_enabled(const struct pbl_regulator *reg) {
  return reg->always_on || (reg->state->use_count > 0);
}
