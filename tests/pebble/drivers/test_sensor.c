/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/sensor.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_mutex.h>

struct pbl_workq pbl_sys_workq_high;
static struct pbl_work *s_work;

void pbl_work_init(struct pbl_work *work, pbl_work_fn_t fn) {
  *work = (struct pbl_work)PBL_WORK_INITIALIZER(fn);
}

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  bool queued = s_work == nullptr;

  s_work = work;
  return queued;
}

static void prv_run_work(void) {
  struct pbl_work *work = s_work;

  s_work = nullptr;
  if (work != nullptr) {
    work->fn(work);
  }
}

/* Frame: accel X, Y, Z as S16 LE in units of 1000 ug, then temperature as U16 BE in 1/4 C. */
static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_TEMP, 0, 6, PBL_SENSOR_FMT_U16_BE, 250, 1, -10000),
};

#define FRAME_SIZE 8U

static int16_t s_xyz[3];
static uint16_t s_temp_raw;
static int s_set_event_calls;

static void prv_put_frame(uint8_t *frame, int16_t x, int16_t y, int16_t z, uint16_t temp) {
  pbl_put_le16((uint16_t)x, &frame[0]);
  pbl_put_le16((uint16_t)y, &frame[2]);
  pbl_put_le16((uint16_t)z, &frame[4]);
  frame[6] = (uint8_t)(temp >> 8);
  frame[7] = (uint8_t)temp;
}

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  prv_put_frame(frame->data, s_xyz[0], s_xyz[1], s_xyz[2], s_temp_raw);
  frame->chans = s_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_chans);
  frame->timestamp_us = 1234U;
  return 0;
}

static int prv_set_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type,
                         bool on) {
  s_set_event_calls++;
  return 0;
}

static int prv_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  return 0;
}

static const struct pbl_sensor_ops s_ops = {
  .read = prv_read,
  .attr_set = prv_attr_set,
  .set_event = prv_set_event,
};

/* Watch X is sensor -Y, watch Y is sensor X, watch Z is sensor Z. */
PBL_SENSOR_STREAM_STATE_DEFINE(s_fake, 2, 4U * FRAME_SIZE);
static const struct pbl_sensor s_fake = PBL_SENSOR_INIT(
    s_fake, "fake", nullptr, &s_ops, PBL_BIT(PBL_SENSOR_ACCEL) | PBL_BIT(PBL_SENSOR_TEMP),
    PBL_BIT(PBL_SENSOR_EVENT_SHAKE), PBL_SENSOR_MOUNT_MATRIX(0, -1, 0, 1, 0, 0, 0, 0, 1));
PBL_DEVICE_REGISTER(s_fake, &s_fake.dev);

static int s_batches;
static int32_t s_decoded[12];
static int s_decoded_n;
static bool s_hold;
static struct pbl_sensor_batch s_held[2];
static int s_num_held;
static struct pbl_sensor_event s_event;
static int s_events;

static void prv_batch(const struct pbl_sensor *sensor, const struct pbl_sensor_batch *batch,
                      void *user_data) {
  s_batches++;
  s_decoded_n =
      pbl_sensor_decode(sensor, batch, PBL_SENSOR_ACCEL, 0U, batch->num_frames, s_decoded);
  if (s_hold) {
    s_held[s_num_held++] = *batch;
    pbl_sensor_hold(sensor, batch);
  }
}

static void prv_event(const struct pbl_sensor *sensor, const struct pbl_sensor_event *event,
                      void *user_data) {
  s_events++;
  s_event = *event;
}

static const struct pbl_sensor_listener s_listener = {.batch = prv_batch, .event = prv_event};

static void prv_report_frames(int n, bool submit) {
  uint8_t *data = pbl_sensor_buf_alloc(&s_fake, nullptr);

  cl_assert(data != nullptr);
  for (int i = 0; i < n; i++) {
    prv_put_frame(&data[i * FRAME_SIZE], (int16_t)(i + 1), (int16_t)(10 * (i + 1)), 100, 0);
  }

  const struct pbl_sensor_batch batch = {
    .data = data,
    .chans = s_chans,
    .num_chans = PBL_ARRAY_SIZE(s_chans),
    .stride = FRAME_SIZE,
    .num_frames = (uint16_t)n,
    .timestamp_us = 100000U,
    .interval_us = 10000U,
  };

  if (submit) {
    pbl_sensor_submit(&s_fake, &batch);
  } else {
    pbl_sensor_lock(&s_fake);
    pbl_sensor_report(&s_fake, &batch);
    pbl_sensor_unlock(&s_fake);
  }
}

void test_sensor__initialize(void) {
  cl_assert_equal_i(pbl_device_init(&s_fake.dev), 0);
  pbl_sensor_set_listener(&s_fake, &s_listener);
  pbl_sensor_set_rotated(&s_fake, false);
  s_xyz[0] = s_xyz[1] = s_xyz[2] = 0;
  s_temp_raw = 0U;
  s_batches = 0;
  s_decoded_n = 0;
  s_hold = false;
  s_num_held = 0;
  s_events = 0;
  s_set_event_calls = 0;
  s_work = nullptr;
}

void test_sensor__cleanup(void) {
  (void)pbl_sensor_set_event(&s_fake, PBL_SENSOR_EVENT_SHAKE, false);
  pbl_sensor_set_listener(&s_fake, nullptr);
  fake_mutex_reset(true /* assert_all_unlocked */);
}

void test_sensor__read_scales_scalars(void) {
  int32_t mc;
  uint64_t ts;

  s_temp_raw = 0x0100U;
  cl_assert_equal_i(pbl_sensor_read(&s_fake, PBL_SENSOR_TEMP, &mc, &ts), 0);
  cl_assert_equal_i(mc, 256 * 250 - 10000);
  cl_assert_equal_i(ts, 1234);
}

void test_sensor__read_applies_mount_and_rotation(void) {
  int32_t xyz[3];

  s_xyz[0] = 1;
  s_xyz[1] = 2;
  s_xyz[2] = 3;
  cl_assert_equal_i(pbl_sensor_read(&s_fake, PBL_SENSOR_ACCEL, xyz, nullptr), 0);
  cl_assert_equal_i(xyz[0], -2000);
  cl_assert_equal_i(xyz[1], 1000);
  cl_assert_equal_i(xyz[2], 3000);

  pbl_sensor_set_rotated(&s_fake, true);
  cl_assert_equal_i(pbl_sensor_read(&s_fake, PBL_SENSOR_ACCEL, xyz, nullptr), 0);
  cl_assert_equal_i(xyz[0], 2000);
  cl_assert_equal_i(xyz[1], -1000);
  cl_assert_equal_i(xyz[2], 3000);
}

void test_sensor__unmeasured_types_are_not_supported(void) {
  int32_t xyz[3];
  int32_t val = 0;

  cl_assert_equal_i(pbl_sensor_read(&s_fake, PBL_SENSOR_GYRO, xyz, nullptr), -ENOTSUP);
  cl_assert_equal_i(pbl_sensor_attr_set(&s_fake, PBL_SENSOR_GYRO, PBL_SENSOR_ATTR_INTERVAL, &val),
                    -ENOTSUP);
  cl_assert_equal_i(pbl_sensor_attr_get(&s_fake, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_INTERVAL, &val),
                    -ENOTSUP);
  cl_assert_equal_i(pbl_sensor_set_event(&s_fake, PBL_SENSOR_EVENT_DOUBLE_TAP, true), -ENOTSUP);
}

void test_sensor__batch_max_is_read_only(void) {
  int32_t val = 1;

  cl_assert_equal_i(pbl_sensor_attr_set(&s_fake, PBL_SENSOR_ACCEL, PBL_SENSOR_ATTR_BATCH_MAX, &val),
                    -EACCES);
}

void test_sensor__reported_batches_are_decoded_and_freed(void) {
  prv_report_frames(3, false);

  cl_assert_equal_i(s_batches, 1);
  cl_assert_equal_i(s_decoded_n, 3);
  cl_assert_equal_i(s_decoded[0], -10000);
  cl_assert_equal_i(s_decoded[1], 1000);
  cl_assert_equal_i(s_decoded[2], 100000);
  cl_assert_equal_i(s_decoded[6], -30000);
  cl_assert_equal_i(s_decoded[7], 3000);

  prv_report_frames(1, false);
  prv_report_frames(1, false);
  cl_assert_equal_i(s_batches, 3);
}

void test_sensor__held_buffers_wait_for_release(void) {
  s_hold = true;
  prv_report_frames(1, false);
  prv_report_frames(1, false);
  cl_assert(pbl_sensor_buf_alloc(&s_fake, nullptr) == nullptr);

  pbl_sensor_release(&s_fake, &s_held[1]);
  s_hold = false;
  prv_report_frames(1, false);
  cl_assert_equal_i(s_batches, 3);
  pbl_sensor_release(&s_fake, &s_held[0]);
}

void test_sensor__submitted_batches_are_reported_in_order_from_work(void) {
  prv_report_frames(1, true);
  prv_report_frames(2, true);
  cl_assert_equal_i(s_batches, 0);

  prv_run_work();
  cl_assert_equal_i(s_batches, 2);
  cl_assert_equal_i(s_decoded_n, 3);
  cl_assert_equal_i(s_decoded[3], -20000);
  cl_assert(pbl_sensor_buf_alloc(&s_fake, nullptr) != nullptr);
}

void test_sensor__frame_times_count_back_from_the_last(void) {
  const struct pbl_sensor_batch batch = {.num_frames = 3, .timestamp_us = 1000, .interval_us = 100};

  cl_assert_equal_i(pbl_sensor_frame_time(&batch, 0), 800);
  cl_assert_equal_i(pbl_sensor_frame_time(&batch, 2), 1000);
}

void test_sensor__events_are_filtered_and_remapped(void) {
  const struct pbl_sensor_event event = {
    .type = PBL_SENSOR_EVENT_SHAKE,
    .axis = PBL_SENSOR_AXIS_Y,
    .direction = 1,
  };

  pbl_sensor_lock(&s_fake);
  pbl_sensor_report_event(&s_fake, &event);
  pbl_sensor_unlock(&s_fake);
  cl_assert_equal_i(s_events, 0);

  cl_assert_equal_i(pbl_sensor_set_event(&s_fake, PBL_SENSOR_EVENT_SHAKE, true), 0);
  cl_assert(pbl_sensor_get_event(&s_fake, PBL_SENSOR_EVENT_SHAKE));
  cl_assert_equal_i(s_set_event_calls, 1);

  pbl_sensor_lock(&s_fake);
  pbl_sensor_report_event(&s_fake, &event);
  pbl_sensor_unlock(&s_fake);
  cl_assert_equal_i(s_events, 1);
  cl_assert_equal_i(s_event.axis, PBL_SENSOR_AXIS_X);
  cl_assert_equal_i(s_event.direction, -1);
}
