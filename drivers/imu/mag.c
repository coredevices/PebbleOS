/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mag.h>
#include <pbl/util/misc.h>

int pbl_mag_dev_init(const struct pbl_device *dev) {
  const struct pbl_mag *mag = container_of(dev, const struct pbl_mag, dev);
  int ret;

  pbl_mag_lock(mag);
  ret = mag->ops->init(mag);
  pbl_mag_unlock(mag);

  return ret;
}

void pbl_mag_lock(const struct pbl_mag *mag) {
  pbl_mutex_lock(&mag->state->lock, PBL_FOREVER);
}

void pbl_mag_unlock(const struct pbl_mag *mag) {
  pbl_mutex_unlock(&mag->state->lock);
}

int pbl_mag_set_listener(const struct pbl_mag *mag, const struct pbl_mag_listener *listener) {
  pbl_mag_lock(mag);
  mag->state->listener = listener;
  pbl_mag_unlock(mag);

  return 0;
}

int pbl_mag_set_rotated(const struct pbl_mag *mag, bool rotated) {
  int ret;

  if (!pbl_device_is_ready(&mag->dev)) {
    return -ENODEV;
  }

  if (mag->ops->set_rotated == nullptr) {
    return 0;
  }

  pbl_mag_lock(mag);
  ret = mag->ops->set_rotated(mag, rotated);
  pbl_mag_unlock(mag);

  return ret;
}

int pbl_mag_set_sampling_interval(const struct pbl_mag *mag, uint32_t interval_us,
                                  uint32_t *actual_us) {
  uint32_t actual;
  int ret;

  if (!pbl_device_is_ready(&mag->dev)) {
    return -ENODEV;
  }

  pbl_mag_lock(mag);
  ret = mag->ops->set_sampling_interval(mag, interval_us, &actual);
  if (ret == 0) {
    mag->state->sampling_interval_us = actual;
  }
  pbl_mag_unlock(mag);
  if (ret != 0) {
    return ret;
  }

  if (actual_us != nullptr) {
    *actual_us = actual;
  }

  return 0;
}

uint32_t pbl_mag_get_sampling_interval(const struct pbl_mag *mag) {
  if (!pbl_device_is_ready(&mag->dev)) {
    return 0;
  }

  return mag->state->sampling_interval_us;
}

int pbl_mag_peek(const struct pbl_mag *mag, MagData *sample) {
  int ret;

  if (!pbl_device_is_ready(&mag->dev)) {
    return -ENODEV;
  }

  pbl_mag_lock(mag);
  ret = mag->ops->peek(mag, sample);
  pbl_mag_unlock(mag);

  return ret;
}

void pbl_mag_report_sample(const struct pbl_mag *mag, const MagData *sample) {
  const struct pbl_mag_listener *listener = mag->state->listener;

  pbl_mutex_assert_held(&mag->state->lock, true);

  if (listener == nullptr || listener->sample == nullptr) {
    return;
  }

  listener->sample(mag, sample, listener->user_data);
}
