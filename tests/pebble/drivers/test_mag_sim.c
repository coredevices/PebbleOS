/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/imu/sim.h>
#include <pbl/drivers/mag.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_rtc.h>
#include <fake_work.h>

static int s_samples;
static MagData s_last;

static void prv_sample(const struct pbl_mag *mag, const MagData *sample, void *user_data) {
  s_samples++;
  s_last = *sample;
}

static const struct pbl_mag_listener s_listener = {.sample = prv_sample};

void test_mag_sim__initialize(void) {
  cl_assert_equal_i(pbl_device_init(&SIM_MAG->dev), 0);
  cl_assert_equal_i(pbl_mag_set_listener(SIM_MAG, &s_listener), 0);
  s_samples = 0;
  s_last = (MagData){};
}

void test_mag_sim__cleanup(void) {
  pbl_mag_set_sampling_interval(SIM_MAG, 0, nullptr);
  pbl_mag_sim_set(SIM_MAG, &(MagData){});
  pbl_mag_set_listener(SIM_MAG, nullptr);
  fake_mutex_reset(true /* assert_all_unlocked */);
  fake_work_cleanup();
}

void test_mag_sim__reports_the_set_field_while_sampling(void) {
  cl_assert_equal_i(pbl_mag_set_sampling_interval(SIM_MAG, 100000, nullptr), 0);
  pbl_mag_sim_set(SIM_MAG, &(MagData){.x = 100, .y = -200, .z = 300});

  fake_work_fire(fake_work_get_next());
  fake_work_fire(fake_work_get_next());

  cl_assert_equal_i(s_samples, 2);
  cl_assert_equal_i(s_last.x, 100);
  cl_assert_equal_i(s_last.y, -200);
  cl_assert_equal_i(s_last.z, 300);
}

void test_mag_sim__stops_when_sampling_stops(void) {
  MagData sample;

  cl_assert_equal_i(pbl_mag_set_sampling_interval(SIM_MAG, 100000, nullptr), 0);
  cl_assert_equal_i(pbl_mag_set_sampling_interval(SIM_MAG, 0, nullptr), 0);

  cl_assert_equal_p(fake_work_get_next(), nullptr);
  cl_assert_equal_i(pbl_mag_peek(SIM_MAG, &sample), -ENODATA);
}

void test_mag_sim__peek_reads_the_field(void) {
  MagData sample;

  cl_assert_equal_i(pbl_mag_set_sampling_interval(SIM_MAG, 100000, nullptr), 0);
  pbl_mag_sim_set(SIM_MAG, &(MagData){.x = 42});

  cl_assert_equal_i(pbl_mag_peek(SIM_MAG, &sample), 0);
  cl_assert_equal_i(sample.x, 42);
}
