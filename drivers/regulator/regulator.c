/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/regulator.h>
#include <pbl/util/misc.h>

#include <system/passert.h>

int pbl_regulator_dev_init(const struct pbl_device *dev) {
  const struct pbl_regulator *reg = container_of(dev, const struct pbl_regulator, dev);
  int ret;

  pbl_mutex_init(&reg->state->mutex);
  reg->state->use_count = 0;

  if (reg->ops->init != nullptr) {
    ret = reg->ops->init(reg);
    if (ret != 0) {
      return ret;
    }
  }

  return reg->always_on ? reg->ops->enable(reg) : reg->ops->disable(reg);
}

static int prv_enable_locked(const struct pbl_regulator *reg) {
  int ret;

  if (reg->state->use_count == 0) {
    ret = reg->ops->enable(reg);
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

bool pbl_regulator_is_enabled(const struct pbl_regulator *reg) {
  return reg->always_on || (reg->state->use_count > 0);
}
