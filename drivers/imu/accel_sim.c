/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/accel.h>
#include <pbl/drivers/imu/sim.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/util/math.h>

static uint32_t s_sampling_interval_ms;
static uint32_t s_num_samples;

static struct pbl_accel_sim_sample s_queue[PBL_ACCEL_SIM_QUEUE_SIZE];
static size_t s_queue_read;
static size_t s_queue_count;
static struct pbl_accel_sim_sample s_latest;

static bool s_timer_running;
static struct pbl_delayed_work s_timer;

static void prv_construct_sample(AccelDriverSample *sample) {
  *sample = (AccelDriverSample){
    .timestamp_us = pbl_realtime_ms() * 1000,
    .x = s_latest.x,
    .y = s_latest.y,
    .z = s_latest.z,
  };
}

static void prv_stop_timer(void) {
  s_timer_running = false;
  pbl_delayed_work_cancel(&s_timer);
}

static void prv_start_timer(void) {
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_timer, PBL_MSEC(s_sampling_interval_ms));
  s_timer_running = true;
}

static void prv_timer_fn(struct pbl_work *work) {
  const struct pbl_accel *accel = SIM_ACCEL;

  pbl_accel_lock(accel);

  if (s_queue_count > 0U) {
    s_latest = s_queue[s_queue_read];
    s_queue_read = (s_queue_read + 1U) % PBL_ACCEL_SIM_QUEUE_SIZE;
    s_queue_count--;
  }

  if (s_num_samples > 0U) {
    AccelDriverSample sample;

    prv_construct_sample(&sample);
    pbl_accel_report_sample(accel, &sample);
  }

  if (s_num_samples == 0U && s_queue_count == 0U) {
    prv_stop_timer();
  } else if (s_timer_running && !pbl_delayed_work_is_pending(&s_timer)) {
    pbl_delayed_work_schedule_next(&s_timer, pbl_ms_to_ticks(s_sampling_interval_ms));
  }

  pbl_accel_unlock(accel);
}

size_t pbl_accel_sim_feed(const struct pbl_accel *accel, const struct pbl_accel_sim_sample *samples,
                          size_t count) {
  pbl_accel_lock(accel);

  count = MIN(count, PBL_ACCEL_SIM_QUEUE_SIZE - s_queue_count);
  for (size_t i = 0U; i < count; i++) {
    s_queue[(s_queue_read + s_queue_count) % PBL_ACCEL_SIM_QUEUE_SIZE] = samples[i];
    s_queue_count++;
  }

  if (!s_timer_running && s_queue_count > 0U) {
    prv_start_timer();
  }

  pbl_accel_unlock(accel);

  return count;
}

void pbl_accel_sim_flush(const struct pbl_accel *accel) {
  pbl_accel_lock(accel);
  s_queue_count = 0U;
  pbl_accel_unlock(accel);
}

void pbl_accel_sim_report_event(const struct pbl_accel *accel,
                                const struct pbl_accel_event *event) {
  bool enabled;

  pbl_accel_lock(accel);

  switch (event->type) {
    case PBL_ACCEL_EVENT_SHAKE:
      enabled = pbl_accel_get_shake_detection(accel);
      break;
    case PBL_ACCEL_EVENT_DOUBLE_TAP:
      enabled = pbl_accel_get_double_tap_detection(accel);
      break;
    default:
      enabled = false;
      break;
  }

  if (enabled) {
    pbl_accel_report_event(accel, event);
  }

  pbl_accel_unlock(accel);
}

static int prv_init(const struct pbl_accel *accel) {
  s_latest = (struct pbl_accel_sim_sample){.x = 0, .y = 0, .z = -1000};
  s_queue_count = 0U;
  pbl_delayed_work_init(&s_timer, prv_timer_fn);

  return 0;
}

static uint32_t prv_get_sampling_interval(const struct pbl_accel *accel) {
  return s_sampling_interval_ms * 1000U;
}

static int prv_set_sampling_interval(const struct pbl_accel *accel, uint32_t interval_us,
                                     uint32_t *actual_us) {
  s_sampling_interval_ms = interval_us / 1000U;

  if (s_timer_running) {
    prv_start_timer();
  }

  *actual_us = prv_get_sampling_interval(accel);

  return 0;
}

static uint32_t prv_get_max_num_samples(const struct pbl_accel *accel) {
  return PBL_ACCEL_SIM_QUEUE_SIZE;
}

static int prv_set_num_samples(const struct pbl_accel *accel, uint32_t num_samples) {
  s_num_samples = num_samples;

  if (num_samples > 0U || s_queue_count > 0U) {
    prv_start_timer();
  } else {
    prv_stop_timer();
  }

  return 0;
}

static int prv_peek(const struct pbl_accel *accel, AccelDriverSample *sample) {
  prv_construct_sample(sample);

  return 0;
}

static int prv_set_detection(const struct pbl_accel *accel, bool on) {
  return 0;
}

static const struct pbl_accel_ops s_ops = {
  .init = prv_init,
  .set_sampling_interval = prv_set_sampling_interval,
  .get_sampling_interval = prv_get_sampling_interval,
  .set_num_samples = prv_set_num_samples,
  .get_max_num_samples = prv_get_max_num_samples,
  .peek = prv_peek,
  .set_shake_detection = prv_set_detection,
  .set_double_tap_detection = prv_set_detection,
};

PBL_ACCEL_STATE_DEFINE(pbl_accel_sim);
const struct pbl_accel pbl_accel_sim = PBL_ACCEL_INIT(pbl_accel_sim, "accel", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_accel_sim, &pbl_accel_sim.dev);
