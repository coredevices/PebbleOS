/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/sim.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#define FRAME_SIZE 6U

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
};

static uint32_t s_sampling_interval_ms;
static uint32_t s_num_samples;

static struct pbl_accel_sim_sample s_queue[PBL_ACCEL_SIM_QUEUE_SIZE];
static size_t s_queue_read;
static size_t s_queue_count;
static struct pbl_accel_sim_sample s_latest;

static bool s_timer_running;
static struct pbl_delayed_work s_timer;

static void prv_put_frame(uint8_t *frame) {
  pbl_put_le16((uint16_t)s_latest.x, &frame[0]);
  pbl_put_le16((uint16_t)s_latest.y, &frame[2]);
  pbl_put_le16((uint16_t)s_latest.z, &frame[4]);
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
  const struct pbl_sensor *sensor = SIM_ACCEL;
  uint8_t *frame;

  pbl_sensor_lock(sensor);

  if (s_queue_count > 0U) {
    s_latest = s_queue[s_queue_read];
    s_queue_read = (s_queue_read + 1U) % PBL_ACCEL_SIM_QUEUE_SIZE;
    s_queue_count--;
  }

  if (s_num_samples > 0U) {
    frame = pbl_sensor_buf_alloc(sensor, nullptr);
    if (frame != nullptr) {
      prv_put_frame(frame);
      pbl_sensor_report(sensor, &(const struct pbl_sensor_batch){
                                  .data = frame,
                                  .chans = s_chans,
                                  .num_chans = PBL_ARRAY_SIZE(s_chans),
                                  .stride = FRAME_SIZE,
                                  .num_frames = 1U,
                                  .timestamp_us = pbl_uptime_us(),
                                  .interval_us = s_sampling_interval_ms * 1000U,
                                });
    }
  }

  if (s_num_samples == 0U && s_queue_count == 0U) {
    prv_stop_timer();
  } else if (s_timer_running && !pbl_delayed_work_is_pending(&s_timer)) {
    pbl_delayed_work_schedule_next(&s_timer, pbl_ms_to_ticks(s_sampling_interval_ms));
  }

  pbl_sensor_unlock(sensor);
}

size_t pbl_accel_sim_feed(const struct pbl_sensor *sensor,
                          const struct pbl_accel_sim_sample *samples, size_t count) {
  pbl_sensor_lock(sensor);

  count = PBL_MIN(count, PBL_ACCEL_SIM_QUEUE_SIZE - s_queue_count);
  for (size_t i = 0U; i < count; i++) {
    s_queue[(s_queue_read + s_queue_count) % PBL_ACCEL_SIM_QUEUE_SIZE] = samples[i];
    s_queue_count++;
  }

  if (!s_timer_running && s_queue_count > 0U) {
    prv_start_timer();
  }

  pbl_sensor_unlock(sensor);

  return count;
}

void pbl_accel_sim_flush(const struct pbl_sensor *sensor) {
  pbl_sensor_lock(sensor);
  s_queue_count = 0U;
  pbl_sensor_unlock(sensor);
}

void pbl_accel_sim_report_event(const struct pbl_sensor *sensor,
                                const struct pbl_sensor_event *event) {
  pbl_sensor_lock(sensor);
  pbl_sensor_report_event(sensor, event);
  pbl_sensor_unlock(sensor);
}

static int prv_init(const struct pbl_sensor *sensor) {
  s_latest = (struct pbl_accel_sim_sample){.x = 0, .y = 0, .z = -1000};
  s_queue_count = 0U;
  pbl_delayed_work_init(&s_timer, prv_timer_fn);

  return 0;
}

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  prv_put_frame(frame->data);
  frame->chans = s_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_chans);
  frame->timestamp_us = pbl_uptime_us();

  return 0;
}

static int prv_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  switch (attr) {
    case PBL_SENSOR_ATTR_INTERVAL:
      if (*val < 0) {
        return -EINVAL;
      }
      s_sampling_interval_ms = (uint32_t)*val / 1000U;
      if (s_timer_running) {
        prv_start_timer();
      }
      *val = (int32_t)(s_sampling_interval_ms * 1000U);
      return 0;
    case PBL_SENSOR_ATTR_BATCH:
      *val = PBL_CLAMP(*val, 0, (int32_t)PBL_ACCEL_SIM_QUEUE_SIZE);
      s_num_samples = (uint32_t)*val;
      if (s_num_samples > 0U || s_queue_count > 0U) {
        prv_start_timer();
      } else {
        prv_stop_timer();
      }
      return 0;
    case PBL_SENSOR_ATTR_SHAKE_SENSITIVITY:
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  switch (attr) {
    case PBL_SENSOR_ATTR_INTERVAL:
      *val = (int32_t)(s_sampling_interval_ms * 1000U);
      return 0;
    case PBL_SENSOR_ATTR_BATCH:
      *val = (int32_t)s_num_samples;
      return 0;
    case PBL_SENSOR_ATTR_BATCH_MAX:
      *val = (int32_t)PBL_ACCEL_SIM_QUEUE_SIZE;
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_set_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type,
                         bool on) {
  return 0;
}

static const struct pbl_sensor_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
  .attr_set = prv_attr_set,
  .attr_get = prv_attr_get,
  .set_event = prv_set_event,
};

PBL_SENSOR_STREAM_STATE_DEFINE(pbl_accel_sim, 2, FRAME_SIZE);
const struct pbl_sensor pbl_accel_sim =
    PBL_SENSOR_INIT(pbl_accel_sim, "accel", nullptr, &s_ops, PBL_BIT(PBL_SENSOR_ACCEL),
                    PBL_BIT(PBL_SENSOR_EVENT_SHAKE) | PBL_BIT(PBL_SENSOR_EVENT_DOUBLE_TAP),
                    PBL_SENSOR_MOUNT_IDENTITY);
PBL_DEVICE_REGISTER(pbl_accel_sim, &pbl_accel_sim.dev);
