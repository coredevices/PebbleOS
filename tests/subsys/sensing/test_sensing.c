/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/sensor.h>
#include <pbl/sensing/sensing.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_pbl_malloc.h>

struct pbl_workq pbl_sys_workq_high;

void pbl_work_init(struct pbl_work *work, pbl_work_fn_t fn) {
  *work = (struct pbl_work)PBL_WORK_INITIALIZER(fn);
}

bool pbl_work_submit(struct pbl_workq *q, struct pbl_work *work) {
  return true;
}

// Fake IMU: accel and gyro as S16 LE, in units of 1000 of the canonical unit

#define FRAME_SIZE 6U
#define BATCH_MAX  32

static const struct pbl_sensor_chan s_xl_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
};

static const struct pbl_sensor_chan s_gyro_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_GYRO, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_GYRO, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_GYRO, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_S16_LE, 1000, 1, 0),
};

static int32_t s_interval_us[PBL_SENSOR_TYPE_COUNT];
static int32_t s_forced_interval_us;
static int32_t s_batch[PBL_SENSOR_TYPE_COUNT];
static int32_t s_last_batch_type;
static bool s_event_on[PBL_SENSOR_EVENT_COUNT];
static int s_event_calls;

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  return -ENODATA;
}

static int prv_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  switch (attr) {
    case PBL_SENSOR_ATTR_INTERVAL:
      if (s_forced_interval_us != 0 && *val != 0) {
        *val = s_forced_interval_us;
      }
      s_interval_us[type] = *val;
      return 0;
    case PBL_SENSOR_ATTR_BATCH:
      s_batch[type] = *val;
      s_last_batch_type = type;
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  if (attr != PBL_SENSOR_ATTR_BATCH_MAX) {
    return -ENOTSUP;
  }

  *val = BATCH_MAX;
  return 0;
}

static int prv_set_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type,
                         bool on) {
  s_event_on[type] = on;
  s_event_calls++;
  return 0;
}

static const struct pbl_sensor_ops s_ops = {
  .read = prv_read,
  .attr_set = prv_attr_set,
  .attr_get = prv_attr_get,
  .set_event = prv_set_event,
};

PBL_SENSOR_STREAM_STATE_DEFINE(s_imu, 2, BATCH_MAX *FRAME_SIZE);
static const struct pbl_sensor s_imu = PBL_SENSOR_INIT(
    s_imu, "imu", nullptr, &s_ops, PBL_BIT(PBL_SENSOR_ACCEL) | PBL_BIT(PBL_SENSOR_GYRO),
    PBL_BIT(PBL_SENSOR_EVENT_SHAKE) | PBL_BIT(PBL_SENSOR_EVENT_DOUBLE_TAP),
    PBL_SENSOR_MOUNT_IDENTITY);

// Frames carry a running counter on X, so readers can check what they got
static uint16_t s_counter[PBL_SENSOR_TYPE_COUNT];
static uint64_t s_now_us[PBL_SENSOR_TYPE_COUNT];

static void prv_feed_type(enum pbl_sensor_type type, uint16_t frames, uint32_t interval_us) {
  pbl_sensor_lock(&s_imu);

  uint8_t *data = pbl_sensor_buf_alloc(&s_imu, nullptr);
  cl_assert(data != nullptr);

  for (uint16_t i = 0U; i < frames; i++) {
    pbl_put_le16(s_counter[type]++, &data[i * FRAME_SIZE]);
    pbl_put_le16(0U, &data[i * FRAME_SIZE + 2U]);
    pbl_put_le16(0U, &data[i * FRAME_SIZE + 4U]);
  }

  s_now_us[type] += (uint64_t)frames * interval_us;

  pbl_sensor_report(&s_imu, &(const struct pbl_sensor_batch){
                              .data = data,
                              .chans = (type == PBL_SENSOR_GYRO) ? s_gyro_chans : s_xl_chans,
                              .num_chans = 3U,
                              .stride = FRAME_SIZE,
                              .num_frames = frames,
                              .timestamp_us = s_now_us[type] - interval_us,
                              .interval_us = interval_us,
                            });

  pbl_sensor_unlock(&s_imu);
}

static void prv_feed(uint16_t frames) {
  uint32_t interval_us = (uint32_t)s_interval_us[PBL_SENSOR_ACCEL];

  while (frames > 0U) {
    uint16_t n = PBL_MIN(frames, (uint16_t)BATCH_MAX);

    prv_feed_type(PBL_SENSOR_ACCEL, n, interval_us);
    frames -= n;
  }
}

static int s_notified;
static bool s_notify_result;

static bool prv_notify(struct pbl_sensing_client *client) {
  s_notified++;
  return s_notify_result;
}

static int s_events_a;
static int s_events_b;

static void prv_event_a(struct pbl_sensing_client *client, const struct pbl_sensor_event *event) {
  s_events_a++;
}

static void prv_event_b(struct pbl_sensing_client *client, const struct pbl_sensor_event *event) {
  s_events_b++;
}

static struct pbl_sensing_client s_a;
static struct pbl_sensing_client s_b;

static void prv_client(struct pbl_sensing_client *c, enum pbl_sensor_type type,
                       uint32_t interval_us, uint16_t batch) {
  *c = (struct pbl_sensing_client){
    .sensor = &s_imu,
    .type = type,
    .interval_us = interval_us,
    .batch = batch,
    .notify = prv_notify,
  };
  cl_assert_equal_i(pbl_sensing_client_add(c), 0);
}

void test_sensing__initialize(void) {
  static bool s_init;

  if (!s_init) {
    cl_assert_equal_i(pbl_device_init(&s_imu.dev), 0);
    s_init = true;
  }

  memset(s_interval_us, 0, sizeof(s_interval_us));
  memset(s_batch, 0, sizeof(s_batch));
  memset(s_event_on, 0, sizeof(s_event_on));
  memset(s_now_us, 0, sizeof(s_now_us));
  s_forced_interval_us = 0;
  s_last_batch_type = -1;
  s_event_calls = 0;
  s_notified = 0;
  s_notify_result = true;
  s_events_a = 0;
  s_events_b = 0;
}

void test_sensing__cleanup(void) {
  pbl_sensing_client_remove(&s_a);
  pbl_sensing_client_remove(&s_b);
  pbl_sensing_suspend(&s_imu, false);
}

void test_sensing__the_shortest_interval_wins(void) {
  prv_client(&s_a, PBL_SENSOR_ACCEL, 100000U, 1U);
  prv_client(&s_b, PBL_SENSOR_ACCEL, 40000U, 1U);
  cl_assert_equal_i(s_interval_us[PBL_SENSOR_ACCEL], 40000);

  pbl_sensing_client_remove(&s_b);
  cl_assert_equal_i(s_interval_us[PBL_SENSOR_ACCEL], 100000);

  pbl_sensing_client_remove(&s_a);
  cl_assert_equal_i(s_interval_us[PBL_SENSOR_ACCEL], 0);
}

void test_sensing__the_shortest_batch_duration_wins(void) {
  prv_client(&s_a, PBL_SENSOR_ACCEL, 100000U, 11U);
  prv_client(&s_b, PBL_SENSOR_ACCEL, 40000U, 22U);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 22);

  pbl_sensing_client_set_batch(&s_a, 3U);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 7);

  pbl_sensing_client_set_batch(&s_a, 0U);
  pbl_sensing_client_set_batch(&s_b, 100U);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], BATCH_MAX);
}

void test_sensing__a_slower_client_gets_evenly_decimated_frames(void) {
  int32_t xyz[25][3];
  uint64_t t_us;

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 25U);
  prv_client(&s_b, PBL_SENSOR_ACCEL, 100000U, 10U);
  uint16_t first = s_counter[PBL_SENSOR_ACCEL];

  prv_feed(25);

  cl_assert_equal_i(pbl_sensing_available(&s_a), 25);
  cl_assert_equal_i(pbl_sensing_available(&s_b), 10);
  cl_assert_equal_i(pbl_sensing_read(&s_b, &xyz[0][0], 25U, &t_us), 10);
  cl_assert_equal_i(t_us, 0);
  // The first frame at or after every 100 ms
  const int expected[] = {0, 3, 5, 8, 10, 13, 15, 18, 20, 23};
  for (size_t i = 0U; i < PBL_ARRAY_SIZE(expected); i++) {
    cl_assert_equal_i(xyz[i][0], (first + expected[i]) * 1000);
  }
  cl_assert_equal_i(pbl_sensing_available(&s_b), 0);
}

void test_sensing__a_rate_that_does_not_divide_drops_one_frame_in_26(void) {
  s_forced_interval_us = 38461;
  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 25U);

  prv_feed(26);

  cl_assert_equal_i(pbl_sensing_sampling_interval(&s_a), 38461);
  cl_assert_equal_i(pbl_sensing_available(&s_a), 25);
}

void test_sensing__a_jitter_free_rate_takes_every_tenth_frame(void) {
  int32_t xyz[4][3];

  s_forced_interval_us = 8000;
  prv_client(&s_a, PBL_SENSOR_ACCEL, 80000U, 1U);
  uint16_t first = s_counter[PBL_SENSOR_ACCEL];

  prv_feed(40);

  cl_assert_equal_i(pbl_sensing_read(&s_a, &xyz[0][0], 4U, nullptr), 4);
  for (int i = 0; i < 4; i++) {
    cl_assert_equal_i(xyz[i][0], (first + 10 * i) * 1000);
  }
}

void test_sensing__an_armed_client_is_notified_once(void) {
  int32_t xyz[5][3];

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 5U);
  pbl_sensing_arm(&s_a, 5U);

  prv_feed(4);
  cl_assert_equal_i(s_notified, 0);
  prv_feed(1);
  cl_assert_equal_i(s_notified, 1);
  prv_feed(5);
  cl_assert_equal_i(s_notified, 1);

  cl_assert_equal_i(pbl_sensing_read(&s_a, &xyz[0][0], 5U, nullptr), 5);
  pbl_sensing_arm(&s_a, 5U);
  cl_assert_equal_i(s_notified, 1);
  prv_feed(1);
  cl_assert_equal_i(s_notified, 2);
}

void test_sensing__a_refused_notification_stays_armed(void) {
  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 1U);
  pbl_sensing_arm(&s_a, 1U);
  s_notify_result = false;

  prv_feed(1);
  prv_feed(1);
  s_notify_result = true;
  prv_feed(1);
  prv_feed(1);

  cl_assert_equal_i(s_notified, 3);
}

void test_sensing__frames_keep_their_timestamps_across_batches(void) {
  int32_t xyz[10][3];
  uint64_t t_us;

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 10U);
  prv_feed(3);
  prv_feed(3);

  cl_assert_equal_i(pbl_sensing_read(&s_a, &xyz[0][0], 2U, &t_us), 2);
  cl_assert_equal_i(t_us, 0);
  cl_assert_equal_i(pbl_sensing_read(&s_a, &xyz[0][0], 10U, &t_us), 4);
  cl_assert_equal_i(t_us, 2 * 40000);
}

void test_sensing__a_client_without_batch_discards_frames(void) {
  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 0U);

  prv_feed(10);

  cl_assert_equal_i(pbl_sensing_available(&s_a), 0);
  cl_assert_equal_i(s_interval_us[PBL_SENSOR_ACCEL], 40000);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 0);
}

void test_sensing__a_lagging_client_loses_its_oldest_frames_alone(void) {
  int32_t xyz[3];
  uint16_t expected;

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 1U);
  prv_client(&s_b, PBL_SENSOR_ACCEL, 40000U, 1U);
  expected = s_counter[PBL_SENSOR_ACCEL];

  for (int i = 0; i < 1000; i++) {
    prv_feed_type(PBL_SENSOR_ACCEL, (uint16_t)(1 + i % 7), 40000U);
    while (pbl_sensing_read(&s_a, xyz, 1U, nullptr) == 1U) {
      cl_assert_equal_i(xyz[0], (int16_t)expected * 1000);
      expected++;
    }
  }

  cl_assert_equal_i(expected, s_counter[PBL_SENSOR_ACCEL]);
  cl_assert_equal_i(pbl_sensing_lost(&s_a), 0);
  cl_assert(pbl_sensing_lost(&s_b) > 0U);
  cl_assert(pbl_sensing_available(&s_b) <= 2U * BATCH_MAX * 2U);

  // The lagging client reads in order from where it is left
  uint16_t prev = 0U;
  bool first = true;
  while (pbl_sensing_read(&s_b, xyz, 1U, nullptr) == 1U) {
    uint16_t v = (uint16_t)(xyz[0] / 1000);
    if (!first) {
      cl_assert_equal_i(v, (uint16_t)(prev + 1U));
    }
    prev = v;
    first = false;
  }
  cl_assert_equal_i((uint16_t)(prev + 1U), s_counter[PBL_SENSOR_ACCEL]);
}

void test_sensing__a_client_reading_in_time_loses_nothing_across_wrapping(void) {
  int32_t xyz[16][3];
  uint16_t expected;

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 16U);
  expected = s_counter[PBL_SENSOR_ACCEL];

  // Contiguous single frames merge into records; the client reads a few at a time, well within
  // the buffer, while it wraps many times
  for (int i = 0; i < 3000; i++) {
    prv_feed_type(PBL_SENSOR_ACCEL, 1U, 40000U);
    if ((i % 16) == 15) {
      uint16_t got = pbl_sensing_read(&s_a, &xyz[0][0], 16U, nullptr);
      cl_assert_equal_i(got, 16);
      for (uint16_t k = 0U; k < got; k++) {
        cl_assert_equal_i(xyz[k][0], (int16_t)expected * 1000);
        expected++;
      }
    }
  }

  cl_assert_equal_i(pbl_sensing_lost(&s_a), 0);
}

void test_sensing__batches_that_are_not_contiguous_survive_wrapping(void) {
  int32_t xyz[BATCH_MAX][3];
  uint16_t expected;
  uint32_t seed = 1U;

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 1U);
  expected = s_counter[PBL_SENSOR_ACCEL];

  for (int i = 0; i < 2000; i++) {
    seed = seed * 1103515245U + 12345U;
    uint16_t n = (uint16_t)(1U + (seed >> 16) % BATCH_MAX);

    // A gap in time starts a new record every time
    s_now_us[PBL_SENSOR_ACCEL] += 1000000U;
    prv_feed_type(PBL_SENSOR_ACCEL, n, 40000U);
    expected += pbl_sensing_lost(&s_a);

    if ((seed >> 8) % 3U != 0U) {
      uint16_t got;
      while ((got = pbl_sensing_read(&s_a, &xyz[0][0], (uint16_t)((seed >> 4) % BATCH_MAX + 1U),
                                     nullptr)) > 0U) {
        for (uint16_t k = 0U; k < got; k++) {
          cl_assert_equal_i(xyz[k][0], (int16_t)expected * 1000);
          expected++;
        }
      }
    }
  }

  cl_assert(expected == s_counter[PBL_SENSOR_ACCEL] ||
            pbl_sensing_available(&s_a) == (uint16_t)(s_counter[PBL_SENSOR_ACCEL] - expected));
}

void test_sensing__events_are_enabled_while_wanted(void) {
  prv_client(&s_a, PBL_SENSOR_ACCEL, 0U, 0U);
  prv_client(&s_b, PBL_SENSOR_ACCEL, 0U, 0U);
  s_a.event = prv_event_a;
  s_b.event = prv_event_b;

  pbl_sensing_client_set_events(&s_a, PBL_BIT(PBL_SENSOR_EVENT_SHAKE));
  pbl_sensing_client_set_events(
      &s_b, PBL_BIT(PBL_SENSOR_EVENT_SHAKE) | PBL_BIT(PBL_SENSOR_EVENT_DOUBLE_TAP));
  cl_assert(s_event_on[PBL_SENSOR_EVENT_SHAKE]);
  cl_assert(s_event_on[PBL_SENSOR_EVENT_DOUBLE_TAP]);
  cl_assert_equal_i(s_event_calls, 2);

  pbl_sensor_lock(&s_imu);
  pbl_sensor_report_event(&s_imu, &(const struct pbl_sensor_event){
                                    .type = PBL_SENSOR_EVENT_DOUBLE_TAP,
                                    .direction = 1,
                                  });
  pbl_sensor_report_event(&s_imu, &(const struct pbl_sensor_event){
                                    .type = PBL_SENSOR_EVENT_SHAKE,
                                    .direction = 1,
                                  });
  pbl_sensor_unlock(&s_imu);
  cl_assert_equal_i(s_events_a, 1);
  cl_assert_equal_i(s_events_b, 2);

  pbl_sensing_client_remove(&s_b);
  cl_assert(s_event_on[PBL_SENSOR_EVENT_SHAKE]);
  cl_assert(!s_event_on[PBL_SENSOR_EVENT_DOUBLE_TAP]);

  pbl_sensing_client_remove(&s_a);
  cl_assert(!s_event_on[PBL_SENSOR_EVENT_SHAKE]);
}

void test_sensing__a_suspended_sensor_samples_nothing(void) {
  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 5U);
  pbl_sensing_client_set_events(&s_a, PBL_BIT(PBL_SENSOR_EVENT_SHAKE));
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 5);

  pbl_sensing_suspend(&s_imu, true);
  cl_assert_equal_i(s_interval_us[PBL_SENSOR_ACCEL], 0);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 0);
  cl_assert(!s_event_on[PBL_SENSOR_EVENT_SHAKE]);

  prv_feed_type(PBL_SENSOR_ACCEL, 5U, 40000U);
  cl_assert_equal_i(pbl_sensing_available(&s_a), 0);

  pbl_sensing_suspend(&s_imu, false);
  cl_assert_equal_i(s_interval_us[PBL_SENSOR_ACCEL], 40000);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 5);
  cl_assert(s_event_on[PBL_SENSOR_EVENT_SHAKE]);
}

void test_sensing__the_latest_frame_needs_no_sensor_access(void) {
  int32_t xyz[3];
  uint64_t t_us;

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 1U);
  prv_feed(3);

  cl_assert_equal_i(pbl_sensing_latest(&s_imu, PBL_SENSOR_ACCEL, xyz, &t_us), 0);
  cl_assert_equal_i(xyz[0], (int16_t)(s_counter[PBL_SENSOR_ACCEL] - 1U) * 1000);
  cl_assert_equal_i(t_us, 2 * 40000);

  pbl_sensing_client_remove(&s_a);
  cl_assert_equal_i(pbl_sensing_latest(&s_imu, PBL_SENSOR_ACCEL, xyz, &t_us), -ENODATA);
}

void test_sensing__types_are_buffered_apart(void) {
  int32_t xyz[3];

  prv_client(&s_a, PBL_SENSOR_ACCEL, 40000U, 25U);
  prv_client(&s_b, PBL_SENSOR_GYRO, 10000U, 100U);

  // A device-wide batch size is set last for the fastest type
  cl_assert_equal_i(s_last_batch_type, PBL_SENSOR_GYRO);
  cl_assert_equal_i(s_batch[PBL_SENSOR_GYRO], 32);
  cl_assert_equal_i(s_batch[PBL_SENSOR_ACCEL], 25);

  prv_feed_type(PBL_SENSOR_GYRO, 8U, 10000U);
  prv_feed_type(PBL_SENSOR_ACCEL, 2U, 40000U);

  cl_assert_equal_i(pbl_sensing_available(&s_a), 2);
  cl_assert_equal_i(pbl_sensing_available(&s_b), 8);
  cl_assert_equal_i(pbl_sensing_read(&s_b, xyz, 1U, nullptr), 1);
  cl_assert_equal_i(xyz[0], (int16_t)(s_counter[PBL_SENSOR_GYRO] - 8U) * 1000);
}

void test_sensing__a_client_of_a_missing_type_is_refused(void) {
  struct pbl_sensing_client c = {
    .sensor = &s_imu,
    .type = PBL_SENSOR_MAGN,
    .interval_us = 100000U,
  };

  cl_assert_equal_i(pbl_sensing_client_add(&c), -ENOTSUP);
}
