/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/regulator.h>
#include <pbl/util/misc.h>

#include <system/passert.h>

int pbl_regulator_dev_init(const struct pbl_device *dev) {
  const struct pbl_regulator *reg = container_of(dev, const struct pbl_regulator, dev);
  int res = 0;

  pbl_mutex_init(&reg->state->mutex);
  reg->state->use_count = 0;

  if (reg->ops->init != NULL) {
    res = reg->ops->init(reg);
  }

  if (res == 0) {
    res = reg->always_on ? reg->ops->enable(reg) : reg->ops->disable(reg);
  }

  return res;
}

int pbl_regulator_enable(const struct pbl_regulator *reg) {
  int res = 0;

  PBL_ASSERTN(pbl_device_is_ready(&reg->dev));

  if (reg->always_on) {
    return 0;
  }

  pbl_mutex_lock(&reg->state->mutex, PBL_FOREVER);

  if (reg->state->use_count == 0) {
    res = reg->ops->enable(reg);
  }
  if (res == 0) {
    reg->state->use_count++;
  }

  pbl_mutex_unlock(&reg->state->mutex);

  return res;
}

int pbl_regulator_disable(const struct pbl_regulator *reg) {
  int res = 0;

  if (reg->always_on) {
    return 0;
  }

  pbl_mutex_lock(&reg->state->mutex, PBL_FOREVER);

  PBL_ASSERTN(reg->state->use_count > 0);
  if (reg->state->use_count == 1) {
    res = reg->ops->disable(reg);
  }
  if (res == 0) {
    reg->state->use_count--;
  }

  pbl_mutex_unlock(&reg->state->mutex);

  return res;
}

bool pbl_regulator_is_enabled(const struct pbl_regulator *reg) {
  return reg->always_on || (reg->state->use_count > 0);
}
