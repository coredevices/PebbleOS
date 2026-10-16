/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/temperature.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_temp_class, "temp");

int pbl_temp_dev_init(const struct pbl_device *dev) {
  const struct pbl_temp *temp = container_of(dev, const struct pbl_temp, dev);
  int ret;

  if (temp->ops->init == nullptr) {
    return 0;
  }

  pbl_mutex_lock(&temp->state->lock, PBL_FOREVER);
  ret = temp->ops->init(temp);
  pbl_mutex_unlock(&temp->state->lock);

  return ret;
}

int pbl_temp_read(const struct pbl_temp *temp, int32_t *millicelsius) {
  int ret;

  if (!pbl_device_is_ready(&temp->dev)) {
    return -ENODEV;
  }

  pbl_mutex_lock(&temp->state->lock, PBL_FOREVER);
  ret = temp->ops->read(temp, millicelsius);
  pbl_mutex_unlock(&temp->state->lock);

  return ret;
}
