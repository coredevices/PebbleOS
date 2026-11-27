/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/sim.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_rtc.h>
#include <fake_work.h>

static int s_samples;
static int32_t s_last[3];

static void prv_batch(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                      void *user_data) {
  s_samples += batch->num_frames;
  cl_assert_equal_i(
      pbl_sensor_decode(sensor, batch, PBL_SENSOR_MAGN, batch->num_frames - 1U, 1U, s_last), 3);
}

static const struct pbl_sensor_listener s_listener = {.batch = prv_batch};

static int prv_set_interval(int32_t interval_us) {
  return pbl_sensor_attr_set(SIM_MAG, PBL_SENSOR_MAGN, PBL_SENSOR_ATTR_INTERVAL, &interval_us);
}

void test_mag_sim__initialize(void) {
  cl_assert_equal_i(pbl_device_init(&SIM_MAG->dev), 0);
  pbl_sensor_set_listener(SIM_MAG, &s_listener);
  s_samples = 0;
  s_last[0] = s_last[1] = s_last[2] = 0;
}

void test_mag_sim__cleanup(void) {
  (void)prv_set_interval(0);
  pbl_mag_sim_set(SIM_MAG, 0, 0, 0);
  pbl_sensor_set_listener(SIM_MAG, nullptr);
  fake_mutex_reset(true /* assert_all_unlocked */);
  fake_work_cleanup();
}

void test_mag_sim__reports_the_set_field_while_sampling(void) {
  cl_assert_equal_i(prv_set_interval(100000), 0);
  pbl_mag_sim_set(SIM_MAG, 10000, -20000, 30000);

  fake_work_fire(fake_work_get_next());
  fake_work_fire(fake_work_get_next());

  cl_assert_equal_i(s_samples, 2);
  cl_assert_equal_i(s_last[0], 10000);
  cl_assert_equal_i(s_last[1], -20000);
  cl_assert_equal_i(s_last[2], 30000);
}

void test_mag_sim__stops_when_sampling_stops(void) {
  int32_t interval_us;

  cl_assert_equal_i(prv_set_interval(100000), 0);
  cl_assert_equal_i(prv_set_interval(0), 0);

  cl_assert_equal_p(fake_work_get_next(), nullptr);
  cl_assert_equal_i(
      pbl_sensor_attr_get(SIM_MAG, PBL_SENSOR_MAGN, PBL_SENSOR_ATTR_INTERVAL, &interval_us), 0);
  cl_assert_equal_i(interval_us, 0);
}

void test_mag_sim__read_measures_the_field(void) {
  int32_t nt[3];

  pbl_mag_sim_set(SIM_MAG, 4200, 0, -100);

  cl_assert_equal_i(pbl_sensor_read(SIM_MAG, PBL_SENSOR_MAGN, nt, nullptr), 0);
  cl_assert_equal_i(nt[0], 4200);
  cl_assert_equal_i(nt[2], -100);
}

void test_mag_sim__rotation_inverts_x_and_y(void) {
  int32_t nt[3];

  pbl_mag_sim_set(SIM_MAG, 1, 2, 3);
  pbl_sensor_set_rotated(SIM_MAG, true);

  cl_assert_equal_i(pbl_sensor_read(SIM_MAG, PBL_SENSOR_MAGN, nt, nullptr), 0);
  cl_assert_equal_i(nt[0], -1);
  cl_assert_equal_i(nt[1], -2);
  cl_assert_equal_i(nt[2], 3);

  pbl_sensor_set_rotated(SIM_MAG, false);
}
