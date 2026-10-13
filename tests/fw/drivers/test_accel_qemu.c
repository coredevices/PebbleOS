/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/imu/qemu.h>

#include <clar.h>
#include <comm/qemu/serial.h>
#include <fake_mutex.h>
#include <fake_new_timer.h>
#include <fake_rtc.h>

void qemu_serial_send(QemuProtocol protocol, const uint8_t *data, uint32_t len) {
}

static int s_samples_delivered;
static bool s_locked_at_delivery;

// The accel manager takes the device lock around its own state, so the listener must run under it
static void prv_samples(const struct pbl_accel *accel, const AccelRawBatch *batch,
                        void *user_data) {
  s_samples_delivered += batch->num_samples;
  s_locked_at_delivery = pbl_mutex_is_owner(&accel->state->lock);
}

static const struct pbl_accel_listener s_listener = {.samples = prv_samples};

void test_accel_qemu__initialize(void) {
  cl_assert_equal_i(pbl_device_init(&QEMU_ACCEL->dev), 0);
  cl_assert_equal_i(pbl_accel_set_listener(QEMU_ACCEL, &s_listener), 0);
  s_samples_delivered = 0;
  s_locked_at_delivery = false;
}

void test_accel_qemu__cleanup(void) {
  pbl_accel_set_num_samples(QEMU_ACCEL, 0);
  pbl_accel_set_listener(QEMU_ACCEL, nullptr);
  fake_mutex_reset(true /* assert_all_unlocked */);
}

void test_accel_qemu__sample_is_delivered_locked(void) {
  cl_assert_equal_i(pbl_accel_set_sampling_interval(QEMU_ACCEL, 40000, nullptr), 0);
  cl_assert_equal_i(pbl_accel_set_num_samples(QEMU_ACCEL, 25), 0);

  stub_new_timer_invoke(1);

  cl_assert_equal_i(s_samples_delivered, 1);
  cl_assert(s_locked_at_delivery);
}
