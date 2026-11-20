/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/pressure.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_pressure_class, "pressure");

int pbl_pressure_dev_init(const struct pbl_device *dev) {
  const struct pbl_pressure *pressure = PBL_CONTAINER_OF(dev, const struct pbl_pressure, dev);
  int ret;

  pbl_mutex_lock(&pressure->state->lock, PBL_FOREVER);
  ret = pressure->ops->init(pressure);
  pbl_mutex_unlock(&pressure->state->lock);

  return ret;
}

int pbl_pressure_read(const struct pbl_pressure *pressure, uint32_t *pa) {
  int ret;

  if (!pbl_device_is_ready(&pressure->dev)) {
    return -ENODEV;
  }

  pbl_mutex_lock(&pressure->state->lock, PBL_FOREVER);
  ret = pressure->ops->read(pressure, pa);
  pbl_mutex_unlock(&pressure->state->lock);

  return ret;
}
