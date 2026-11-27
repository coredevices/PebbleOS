/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/sim.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

#define FRAME_SIZE 12U

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_MAGN, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S32_LE, 1, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_MAGN, PBL_SENSOR_AXIS_Y, 4, PBL_SENSOR_FMT_S32_LE, 1, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_MAGN, PBL_SENSOR_AXIS_Z, 8, PBL_SENSOR_FMT_S32_LE, 1, 1, 0),
};

static int32_t s_field[3];
static int32_t s_interval_us;
static struct pbl_delayed_work s_timer;

static void prv_put_frame(uint8_t *frame) {
  for (uint8_t i = 0U; i < 3U; i++) {
    pbl_put_le32((uint32_t)s_field[i], &frame[4U * i]);
  }
}

static void prv_timer_fn(struct pbl_work *work) {
  const struct pbl_sensor *sensor = SIM_MAG;
  uint8_t *frame;

  pbl_sensor_lock(sensor);

  if (s_interval_us != 0) {
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
                                  .interval_us = (uint32_t)s_interval_us,
                                });
    }

    if (!pbl_delayed_work_is_pending(&s_timer)) {
      pbl_delayed_work_schedule_next(&s_timer, pbl_ms_to_ticks((uint32_t)s_interval_us / 1000U));
    }
  }

  pbl_sensor_unlock(sensor);
}

void pbl_mag_sim_set(const struct pbl_sensor *sensor, int32_t x, int32_t y, int32_t z) {
  pbl_sensor_lock(sensor);
  s_field[0] = x;
  s_field[1] = y;
  s_field[2] = z;
  pbl_sensor_unlock(sensor);
}

static int prv_init(const struct pbl_sensor *sensor) {
  s_field[0] = s_field[1] = s_field[2] = 0;
  s_interval_us = 0;
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
  if (attr != PBL_SENSOR_ATTR_INTERVAL) {
    return -ENOTSUP;
  }

  if (*val < 0) {
    return -EINVAL;
  }

  s_interval_us = *val;

  if (s_interval_us == 0) {
    pbl_delayed_work_cancel(&s_timer);
  } else {
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_timer,
                                PBL_MSEC((uint32_t)s_interval_us / 1000U));
  }

  return 0;
}

static int prv_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  if (attr != PBL_SENSOR_ATTR_INTERVAL) {
    return -ENOTSUP;
  }

  *val = s_interval_us;

  return 0;
}

static const struct pbl_sensor_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
  .attr_set = prv_attr_set,
  .attr_get = prv_attr_get,
};

PBL_SENSOR_STREAM_STATE_DEFINE(pbl_mag_sim, 2, FRAME_SIZE);
const struct pbl_sensor pbl_mag_sim = PBL_SENSOR_INIT(
    pbl_mag_sim, "mag", nullptr, &s_ops, PBL_BIT(PBL_SENSOR_MAGN), 0U, PBL_SENSOR_MOUNT_IDENTITY);
PBL_DEVICE_REGISTER(pbl_mag_sim, &pbl_mag_sim.dev);
