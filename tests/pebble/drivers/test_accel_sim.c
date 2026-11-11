/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/drivers/accel.h>
#include <pbl/drivers/imu/sim.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_rtc.h>
#include <fake_work.h>

static int s_samples_delivered;
static int16_t s_last_x;
static bool s_locked_at_delivery;
static int s_events_delivered;

// The accel manager takes the device lock around its own state, so the listener must run under it
static void prv_samples(const struct pbl_accel *accel, const AccelRawBatch *batch,
                        void *user_data) {
  int16_t x;

  memcpy(&x, &batch->data[batch->axis[AXIS_X].offset], sizeof(x));
  s_last_x = x;
  s_samples_delivered += batch->num_samples;
  s_locked_at_delivery = pbl_mutex_is_owner(&accel->state->lock);
}

static void prv_event(const struct pbl_accel *accel, const struct pbl_accel_event *event,
                      void *user_data) {
  s_events_delivered++;
}

static const struct pbl_accel_listener s_listener = {.samples = prv_samples, .event = prv_event};

static void prv_fire(void) {
  fake_work_fire(fake_work_get_next());
}

void test_accel_sim__initialize(void) {
  cl_assert_equal_i(pbl_device_init(&SIM_ACCEL->dev), 0);
  cl_assert_equal_i(pbl_accel_set_listener(SIM_ACCEL, &s_listener), 0);
  cl_assert_equal_i(pbl_accel_set_sampling_interval(SIM_ACCEL, 40000, nullptr), 0);
  s_samples_delivered = 0;
  s_last_x = 0;
  s_locked_at_delivery = false;
  s_events_delivered = 0;
}

void test_accel_sim__cleanup(void) {
  pbl_accel_sim_flush(SIM_ACCEL);
  pbl_accel_set_num_samples(SIM_ACCEL, 0);
  pbl_accel_set_shake_detection(SIM_ACCEL, false);
  pbl_accel_set_listener(SIM_ACCEL, nullptr);
  fake_mutex_reset(true /* assert_all_unlocked */);
  fake_work_cleanup();
}

void test_accel_sim__sample_is_delivered_locked(void) {
  cl_assert_equal_i(pbl_accel_set_num_samples(SIM_ACCEL, 25), 0);

  prv_fire();

  cl_assert_equal_i(s_samples_delivered, 1);
  cl_assert(s_locked_at_delivery);
}

void test_accel_sim__fed_samples_replay_in_order(void) {
  const struct pbl_accel_sim_sample samples[] = {{.x = 1}, {.x = 2}};

  cl_assert_equal_i(pbl_accel_set_num_samples(SIM_ACCEL, 1), 0);
  cl_assert_equal_i(pbl_accel_sim_feed(SIM_ACCEL, samples, 2), 2);

  prv_fire();
  cl_assert_equal_i(s_last_x, 1);
  prv_fire();
  cl_assert_equal_i(s_last_x, 2);
  prv_fire();
  cl_assert_equal_i(s_last_x, 2);
  cl_assert_equal_i(s_samples_delivered, 3);
}

void test_accel_sim__feed_latches_without_subscriber(void) {
  const struct pbl_accel_sim_sample sample = {.x = 7};
  AccelDriverSample peeked;

  cl_assert_equal_i(pbl_accel_sim_feed(SIM_ACCEL, &sample, 1), 1);
  prv_fire();

  cl_assert_equal_p(fake_work_get_next(), nullptr);
  cl_assert_equal_i(pbl_accel_peek(SIM_ACCEL, &peeked), 0);
  cl_assert_equal_i(peeked.x, 7);
  cl_assert_equal_i(s_samples_delivered, 0);
}

void test_accel_sim__feed_drops_what_does_not_fit(void) {
  static struct pbl_accel_sim_sample samples[PBL_ACCEL_SIM_QUEUE_SIZE + 1U];

  cl_assert_equal_i(pbl_accel_sim_feed(SIM_ACCEL, samples, PBL_ACCEL_SIM_QUEUE_SIZE + 1U),
                    PBL_ACCEL_SIM_QUEUE_SIZE);
  cl_assert_equal_i(pbl_accel_sim_feed(SIM_ACCEL, samples, 1), 0);
}

void test_accel_sim__flush_drops_queued_samples(void) {
  const struct pbl_accel_sim_sample samples[] = {{.x = 1}, {.x = 2}};
  const struct pbl_accel_sim_sample sample = {.x = 3};

  cl_assert_equal_i(pbl_accel_set_num_samples(SIM_ACCEL, 1), 0);
  cl_assert_equal_i(pbl_accel_sim_feed(SIM_ACCEL, samples, 2), 2);
  prv_fire();

  pbl_accel_sim_flush(SIM_ACCEL);
  cl_assert_equal_i(pbl_accel_sim_feed(SIM_ACCEL, &sample, 1), 1);
  prv_fire();

  cl_assert_equal_i(s_last_x, 3);
}

void test_accel_sim__event_needs_detection(void) {
  const struct pbl_accel_event event = {
    .type = PBL_ACCEL_EVENT_SHAKE,
    .axis = AXIS_Z,
    .direction = 1,
  };

  pbl_accel_sim_report_event(SIM_ACCEL, &event);
  cl_assert_equal_i(s_events_delivered, 0);

  cl_assert_equal_i(pbl_accel_set_shake_detection(SIM_ACCEL, true), 0);
  pbl_accel_sim_report_event(SIM_ACCEL, &event);
  cl_assert_equal_i(s_events_delivered, 1);
}
