/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/accel.h>
#include <pbl/drivers/imu/posix.h>
#include <pbl/drivers/rtc.h>

// A watch lying still, face up. No FIFO: samples are only peeked.

static uint32_t s_sampling_interval_us = 40000;

static int prv_init(const struct pbl_accel *accel) {
  return 0;
}

static int prv_set_sampling_interval(const struct pbl_accel *accel, uint32_t interval_us,
                                     uint32_t *actual_us) {
  s_sampling_interval_us = interval_us;
  *actual_us = s_sampling_interval_us;

  return 0;
}

static uint32_t prv_get_sampling_interval(const struct pbl_accel *accel) {
  return s_sampling_interval_us;
}

static int prv_set_num_samples(const struct pbl_accel *accel, uint32_t num_samples) {
  return 0;
}

static uint32_t prv_get_max_num_samples(const struct pbl_accel *accel) {
  return 0;
}

static int prv_peek(const struct pbl_accel *accel, AccelDriverSample *sample) {
  time_t s;
  uint16_t ms;

  rtc_get_time_ms(&s, &ms);
  *sample = (AccelDriverSample){
    .timestamp_us = ((uint64_t)s * 1000 + ms) * 1000,
    .x = 0,
    .y = 0,
    .z = -1000,
  };

  return 0;
}

static const struct pbl_accel_ops s_ops = {
  .init = prv_init,
  .set_sampling_interval = prv_set_sampling_interval,
  .get_sampling_interval = prv_get_sampling_interval,
  .set_num_samples = prv_set_num_samples,
  .get_max_num_samples = prv_get_max_num_samples,
  .peek = prv_peek,
};

PBL_ACCEL_STATE_DEFINE(pbl_accel_posix);
const struct pbl_accel pbl_accel_posix = PBL_ACCEL_INIT(pbl_accel_posix, "accel", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_accel_posix, &pbl_accel_posix.dev);
