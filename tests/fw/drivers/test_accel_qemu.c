/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/accel.h>

#include <clar.h>
#include <comm/qemu/serial.h>
#include <fake_mutex.h>
#include <fake_new_timer.h>
#include <fake_rtc.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

void qemu_serial_send(QemuProtocol protocol, const uint8_t *data, uint32_t len) {
}

static int s_samples_delivered;
static bool s_unlocked_at_delivery;

// The accel manager takes its own mutex in here, so the driver must not hold one
void accel_cb_new_sample(AccelDriverSample const *data) {
  s_samples_delivered++;
  s_unlocked_at_delivery = fake_mutex_all_unlocked();
}

void test_accel_qemu__initialize(void) {
  accel_init();
  s_samples_delivered = 0;
  s_unlocked_at_delivery = false;
}

void test_accel_qemu__cleanup(void) {
  accel_set_num_samples(0);
  fake_mutex_reset(true /* assert_all_unlocked */);
}

//! The sample is handed over unlocked, since the manager locks its mutex before calling the driver
void test_accel_qemu__sample_is_delivered_unlocked(void) {
  accel_set_sampling_interval(40000);
  accel_set_num_samples(25);

  stub_new_timer_invoke(1);

  cl_assert_equal_i(s_samples_delivered, 1);
  cl_assert(s_unlocked_at_delivery);
}
