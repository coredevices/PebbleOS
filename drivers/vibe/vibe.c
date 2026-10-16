/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/vibe.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_vibe_class, "vibe");

static void prv_lock(const struct pbl_vibe *vibe) {
  pbl_mutex_lock(&vibe->state->lock, PBL_FOREVER);
}

static void prv_unlock(const struct pbl_vibe *vibe) {
  pbl_mutex_unlock(&vibe->state->lock);
}

int pbl_vibe_dev_init(const struct pbl_device *dev) {
  const struct pbl_vibe *vibe = container_of(dev, const struct pbl_vibe, dev);
  int ret;

  prv_lock(vibe);
  ret = vibe->ops->init(vibe);
  prv_unlock(vibe);

  return ret;
}

int pbl_vibe_start(const struct pbl_vibe *vibe, int8_t strength) {
  int ret;

  if (!pbl_device_is_ready(&vibe->dev)) {
    return -ENODEV;
  }

  prv_lock(vibe);
  ret = vibe->ops->start(vibe, strength);
  prv_unlock(vibe);

  return ret;
}

int pbl_vibe_stop(const struct pbl_vibe *vibe) {
  int ret;

  if (!pbl_device_is_ready(&vibe->dev)) {
    return -ENODEV;
  }

  prv_lock(vibe);
  ret = vibe->ops->stop(vibe);
  prv_unlock(vibe);

  return ret;
}

int8_t pbl_vibe_get_braking_strength(const struct pbl_vibe *vibe) {
  return vibe->braking_strength;
}

int pbl_vibe_calibrate(const struct pbl_vibe *vibe, uint8_t *cali) {
  int ret;

  if (!pbl_device_is_ready(&vibe->dev)) {
    return -ENODEV;
  }

  if (vibe->ops->calibrate == nullptr) {
    return -ENOTSUP;
  }

  *cali = PBL_VIBE_CALIBRATION_NONE;

  prv_lock(vibe);
  ret = vibe->ops->calibrate(vibe, cali);
  prv_unlock(vibe);

  return ret;
}

int pbl_vibe_set_calibration(const struct pbl_vibe *vibe, uint8_t cali) {
  int ret;

  if (!pbl_device_is_ready(&vibe->dev)) {
    return -ENODEV;
  }

  if (vibe->ops->set_calibration == nullptr) {
    return -ENOTSUP;
  }

  prv_lock(vibe);
  ret = vibe->ops->set_calibration(vibe, cali);
  prv_unlock(vibe);

  return ret;
}
