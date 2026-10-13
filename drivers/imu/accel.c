/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/accel.h>
#include <pbl/util/misc.h>

int pbl_accel_dev_init(const struct pbl_device *dev) {
  const struct pbl_accel *accel = container_of(dev, const struct pbl_accel, dev);
  int ret;

  pbl_accel_lock(accel);
  ret = accel->ops->init(accel);
  pbl_accel_unlock(accel);

  return ret;
}

void pbl_accel_lock(const struct pbl_accel *accel) {
  pbl_mutex_lock(&accel->state->lock, PBL_FOREVER);
}

void pbl_accel_unlock(const struct pbl_accel *accel) {
  pbl_mutex_unlock(&accel->state->lock);
}

int pbl_accel_set_listener(const struct pbl_accel *accel,
                           const struct pbl_accel_listener *listener) {
  pbl_accel_lock(accel);
  accel->state->listener = listener;
  pbl_accel_unlock(accel);

  return 0;
}

int pbl_accel_set_rotated(const struct pbl_accel *accel, bool rotated) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  if (accel->ops->set_rotated == nullptr) {
    return 0;
  }

  pbl_accel_lock(accel);
  ret = accel->ops->set_rotated(accel, rotated);
  pbl_accel_unlock(accel);

  return ret;
}

int pbl_accel_set_sampling_interval(const struct pbl_accel *accel, uint32_t interval_us,
                                    uint32_t *actual_us) {
  uint32_t actual;
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  pbl_accel_lock(accel);
  ret = accel->ops->set_sampling_interval(accel, interval_us, &actual);
  pbl_accel_unlock(accel);
  if (ret != 0) {
    return ret;
  }

  if (actual_us != nullptr) {
    *actual_us = actual;
  }

  return 0;
}

uint32_t pbl_accel_get_sampling_interval(const struct pbl_accel *accel) {
  uint32_t interval_us;

  if (!pbl_device_is_ready(&accel->dev)) {
    return 0;
  }

  pbl_accel_lock(accel);
  interval_us = accel->ops->get_sampling_interval(accel);
  pbl_accel_unlock(accel);

  return interval_us;
}

int pbl_accel_set_num_samples(const struct pbl_accel *accel, uint32_t num_samples) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  pbl_accel_lock(accel);
  ret = accel->ops->set_num_samples(accel, num_samples);
  pbl_accel_unlock(accel);

  return ret;
}

uint32_t pbl_accel_get_max_num_samples(const struct pbl_accel *accel) {
  if (!pbl_device_is_ready(&accel->dev)) {
    return 0;
  }

  return accel->ops->get_max_num_samples(accel);
}

int pbl_accel_peek(const struct pbl_accel *accel, AccelDriverSample *sample) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  pbl_accel_lock(accel);
  ret = accel->ops->peek(accel, sample);
  pbl_accel_unlock(accel);

  return ret;
}

static int prv_set_detection_locked(const struct pbl_accel *accel,
                                    int (*set)(const struct pbl_accel *accel, bool on), bool on,
                                    bool *enabled) {
  int ret;

  ret = set(accel, on);
  if (ret != 0) {
    return ret;
  }

  *enabled = on;

  return 0;
}

int pbl_accel_set_shake_detection(const struct pbl_accel *accel, bool on) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  if (accel->ops->set_shake_detection == nullptr) {
    return -ENOTSUP;
  }

  pbl_accel_lock(accel);
  ret = prv_set_detection_locked(accel, accel->ops->set_shake_detection, on,
                                 &accel->state->shake_detection);
  pbl_accel_unlock(accel);

  return ret;
}

bool pbl_accel_get_shake_detection(const struct pbl_accel *accel) {
  return accel->state->shake_detection;
}

int pbl_accel_set_double_tap_detection(const struct pbl_accel *accel, bool on) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  if (accel->ops->set_double_tap_detection == nullptr) {
    return -ENOTSUP;
  }

  pbl_accel_lock(accel);
  ret = prv_set_detection_locked(accel, accel->ops->set_double_tap_detection, on,
                                 &accel->state->double_tap_detection);
  pbl_accel_unlock(accel);

  return ret;
}

bool pbl_accel_get_double_tap_detection(const struct pbl_accel *accel) {
  return accel->state->double_tap_detection;
}

int pbl_accel_set_shake_sensitivity_high(const struct pbl_accel *accel, bool high) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  if (accel->ops->set_shake_sensitivity_high == nullptr) {
    return 0;
  }

  pbl_accel_lock(accel);
  ret = accel->ops->set_shake_sensitivity_high(accel, high);
  pbl_accel_unlock(accel);

  return ret;
}

int pbl_accel_set_shake_sensitivity_percent(const struct pbl_accel *accel, uint8_t percent) {
  int ret;

  if (!pbl_device_is_ready(&accel->dev)) {
    return -ENODEV;
  }

  if (accel->ops->set_shake_sensitivity_percent == nullptr) {
    return 0;
  }

  pbl_accel_lock(accel);
  ret = accel->ops->set_shake_sensitivity_percent(accel, percent);
  pbl_accel_unlock(accel);

  return ret;
}

void pbl_accel_report_samples(const struct pbl_accel *accel, const AccelRawBatch *batch) {
  const struct pbl_accel_listener *listener = accel->state->listener;

  pbl_mutex_assert_held(&accel->state->lock, true);

  if (listener == nullptr || listener->samples == nullptr) {
    return;
  }

  listener->samples(accel, batch, listener->user_data);
}

void pbl_accel_report_sample(const struct pbl_accel *accel, const AccelDriverSample *sample) {
  const AccelRawBatch batch = {
    .data = (const uint8_t *)&sample->x,
    .num_samples = 1,
    .stride = 3 * sizeof(int16_t),
    .axis =
        {
          [AXIS_X] = {.offset = 0, .sign = 1},
          [AXIS_Y] = {.offset = sizeof(int16_t), .sign = 1},
          [AXIS_Z] = {.offset = 2 * sizeof(int16_t), .sign = 1},
        },
    .scale_num = 1,
    .scale_den = 1,
    .first_timestamp_us = sample->timestamp_us,
  };

  pbl_accel_report_samples(accel, &batch);
}

void pbl_accel_report_event(const struct pbl_accel *accel, const struct pbl_accel_event *event) {
  const struct pbl_accel_listener *listener = accel->state->listener;

  pbl_mutex_assert_held(&accel->state->lock, true);

  if (listener == nullptr || listener->event == nullptr) {
    return;
  }

  listener->event(accel, event, listener->user_data);
}
