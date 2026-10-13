/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/drivers/accel.h>
#include <pbl/kernel/compiler.h>
#include <pbl/services/event_service.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_app_manager.h>
#include <fake_new_timer.h>
#include <fake_pbl_malloc.h>
#include <fake_pebble_tasks.h>
#include <fake_system_task.h>

// helpers from accel manager
extern void test_accel_manager_get_subsample_info(AccelManagerState *state, uint16_t *num,
                                                  uint16_t *den, uint16_t *samps_per_update);
extern void test_accel_manager_reset(void);

// stub
void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback start_cb,
                        EventServiceRemoveSubscriberCallback stop_cb) {
}
void sys_vibe_history_start_collecting(void) {
}
void sys_vibe_history_stop_collecting(void) {
}
int32_t sys_vibe_get_vibe_strength(void) {
  return 0;
}
int32_t vibes_get_vibe_strength(void) {
  return 0;
}
uint32_t vibes_get_time_since_last_vibe_ms(void) {
  return UINT32_MAX;
}
bool shell_prefs_get_accel_shake_log_info_enabled(void) {
  return false;
}
struct pbl_msgq *pebble_task_get_to_queue(PebbleTask task) {
  return nullptr;
}

// Fake accelerometer, through the real class layer
static int s_sampling_interval_us = 1000000 / ACCEL_SAMPLING_25HZ;
static int s_num_samples = 0;

//! If true, ignore attempts to change the sampling interval
static bool s_force_sampling_interval;

static int prv_init(const struct pbl_accel *accel) {
  return 0;
}

static int prv_set_sampling_interval(const struct pbl_accel *accel, uint32_t interval_us,
                                     uint32_t *actual_us) {
  if (!s_force_sampling_interval) {
    s_sampling_interval_us = interval_us;
  }
  *actual_us = s_sampling_interval_us;
  return 0;
}

static uint32_t prv_get_sampling_interval(const struct pbl_accel *accel) {
  return s_sampling_interval_us;
}

static int prv_set_num_samples(const struct pbl_accel *accel, uint32_t num_samples) {
  s_num_samples = num_samples;
  return 0;
}

static uint32_t prv_get_max_num_samples(const struct pbl_accel *accel) {
  return 32;
}

static int prv_peek(const struct pbl_accel *accel, AccelDriverSample *sample) {
  return 0;
}

static int prv_set_detection(const struct pbl_accel *accel, bool on) {
  return 0;
}

static const struct pbl_accel_ops s_fake_ops = {
  .init = prv_init,
  .set_sampling_interval = prv_set_sampling_interval,
  .get_sampling_interval = prv_get_sampling_interval,
  .set_num_samples = prv_set_num_samples,
  .get_max_num_samples = prv_get_max_num_samples,
  .peek = prv_peek,
  .set_shake_detection = prv_set_detection,
  .set_double_tap_detection = prv_set_detection,
};

PBL_ACCEL_STATE_DEFINE(s_fake_accel);
static const struct pbl_accel s_fake_accel =
    PBL_ACCEL_INIT(s_fake_accel, "accel", nullptr, &s_fake_ops);
const struct pbl_accel *const ACCEL = &s_fake_accel;

// Data events for a NewTimers subscriber land here, so a test can count them
static int s_posted_events;
static bool s_new_timer_queue_full;

void new_timer_add_work_callback_from_isr(NewTimerWorkCallback cb, void *data) {
}
bool new_timer_add_work_callback(NewTimerWorkCallback cb, void *data) {
  if (s_new_timer_queue_full) {
    return false;
  }
  s_posted_events++;
  return true;
}

// Unit Test Code

void test_accel_manager__initialize(void) {
  cl_assert_equal_i(pbl_device_init(&s_fake_accel.dev), 0);
  accel_manager_init();

  s_sampling_interval_us = 1000000 / ACCEL_SAMPLING_25HZ;
  s_num_samples = 0;
  s_force_sampling_interval = false;
  s_posted_events = 0;
  s_new_timer_queue_full = false;
}

void test_accel_manager__cleanup(void) {
  test_accel_manager_reset();
  fake_system_task_callbacks_cleanup();
}

static void prv_noop_sample_handler(void *context) {
}

static void prv_validate_sample_rates(int *arr, int num_samples) {
  for (int i = 0; i < num_samples; i++) {
    // force a compiler error if user has not added all possible sample rates to array
    switch ((AccelSamplingRate)arr[i]) {
      case ACCEL_SAMPLING_10HZ:
      case ACCEL_SAMPLING_25HZ:
      case ACCEL_SAMPLING_50HZ:
      case ACCEL_SAMPLING_100HZ:
        break;
      default:
        cl_assert(0);
    }
  }
}

static void prv_run_accel_test(int *sample_arr, int num_items) {
  PebbleTask tasks[] = {PebbleTask_KernelMain, PebbleTask_Worker, PebbleTask_App};
  AccelManagerState *sessions[3];

  if (num_items > 3) {
    return; // we only support 3 simultaneous subscribers
  }

  int fastest_rate = 0;
  AccelRawData fake_buf[1];
  for (int i = 0; i < num_items; i++) {
    if (fastest_rate < sample_arr[i]) {
      fastest_rate = sample_arr[i];
    }

    sessions[i] =
        sys_accel_manager_data_subscribe(sample_arr[i], prv_noop_sample_handler, nullptr, tasks[i]);

    // buffer size of 1
    sys_accel_manager_set_sample_buffer(sessions[i], fake_buf, 1);
  }

  //  make sure all sampling rates are what they should be
  for (int i = 0; i < num_items; i++) {
    stub_pebble_tasks_set_current(tasks[i]);

    uint16_t num, den, samps_per_update;
    test_accel_manager_get_subsample_info(sessions[i], &num, &den, &samps_per_update);

    if ((fastest_rate % sample_arr[i]) == 0) {
      // the current sample rate is a multiple of the rate we are running at
      cl_assert_equal_i(num, 1);
      cl_assert_equal_i(den, fastest_rate / sample_arr[i]);
      cl_assert_equal_i(samps_per_update, 1);
    } else {
      // the sample rate is not an even multiple of our fastest rate
      uint32_t gcd_of_rates = gcd(fastest_rate, sample_arr[i]);
      cl_assert_equal_i(num, sample_arr[i] / gcd_of_rates);
      cl_assert_equal_i(den, fastest_rate / gcd_of_rates);
      cl_assert_equal_i(samps_per_update, 1);
    }
  }

  cl_assert_equal_i(1000000 / s_sampling_interval_us, fastest_rate);
  cl_assert_equal_i(s_num_samples, 1);

  for (int i = 0; i < num_items; i++) {
    sys_accel_manager_data_unsubscribe(sessions[i]);
    stub_pebble_tasks_set_current(tasks[i]);
  }
}

// enumerate through all possible sampling rate combinations and confirm
// that the correct frequency is selected
void test_accel_manager__subscription_sampling_rates(void) {
  int sample_rates[] = {
    ACCEL_SAMPLING_10HZ, ACCEL_SAMPLING_25HZ, ACCEL_SAMPLING_50HZ, ACCEL_SAMPLING_100HZ
  };
  prv_validate_sample_rates(sample_rates, ARRAY_LENGTH(sample_rates));

  int poss_rates = ARRAY_LENGTH(sample_rates);
  int max_permutations = 0x1 << poss_rates;

  for (int mask = 0; mask < max_permutations; mask++) {
    int count = PBL_POPCOUNT(mask);
    if (count == 0) {
      continue; // we don't care about the empty set
    }

    int test_rates[count];
    int idx = 0;
    for (int j = 0; j < poss_rates; j++) {
      if ((mask & (0x1 << j)) != 0) {
        test_rates[idx] = sample_rates[j];
        idx++;
      }
    }

    printf("Testing: ");
    for (int i = 0; i < count; i++) {
      printf("%d ", sample_rates[i]);
    }
    printf("\n");

    prv_run_accel_test(test_rates, count);
  }
}

void test_accel_manager__jitterfree(void) {
  // Force the fake accel to only support the 125hz sample rate.
  s_force_sampling_interval = true;
  s_sampling_interval_us = (1000000000 / 125000);

  AccelRawData fake_buf[1];

  AccelManagerState *state = sys_accel_manager_data_subscribe(
      ACCEL_SAMPLING_25HZ, prv_noop_sample_handler, nullptr, PebbleTask_KernelMain);
  uint32_t resulting_mhz = accel_manager_set_jitterfree_sampling_rate(state, 12500);
  sys_accel_manager_set_sample_buffer(state, fake_buf, ARRAY_LENGTH(fake_buf));

  cl_assert_equal_i(resulting_mhz, 12500);

  uint16_t num, den, samples_per_update;
  test_accel_manager_get_subsample_info(state, &num, &den, &samples_per_update);

  cl_assert_equal_i(num, 1);
  cl_assert_equal_i(den, 10);
  cl_assert_equal_i(samples_per_update, ARRAY_LENGTH(fake_buf));
}

void test_accel_manager__batched_samples(void) {
  AccelRawData fake_buf[30];

  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  AccelManagerState *main_session = sys_accel_manager_data_subscribe(
      ACCEL_SAMPLING_10HZ, prv_noop_sample_handler, nullptr, PebbleTask_KernelMain);
  sys_accel_manager_set_sample_buffer(main_session, fake_buf, 11);

  stub_pebble_tasks_set_current(PebbleTask_Worker);
  AccelManagerState *worker_session = sys_accel_manager_data_subscribe(
      ACCEL_SAMPLING_25HZ, prv_noop_sample_handler, nullptr, PebbleTask_KernelMain);
  sys_accel_manager_set_sample_buffer(worker_session, fake_buf, 22);

  cl_assert_equal_i(s_num_samples, 22);

  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  sys_accel_manager_set_sample_buffer(main_session, fake_buf, 3);
  cl_assert_equal_i(s_num_samples, 7); /* 300ms / (1000ms / 25 samps) */
}

// Data events
//////////////////////////////////////////

static uint64_t s_now_us;

// Feeds samples in from the driver, one sampling interval apart
static void prv_feed(int count) {
  for (int i = 0; i < count; i++) {
    AccelDriverSample sample = {.x = i, .timestamp_us = s_now_us};
    s_now_us += 1000000 / ACCEL_SAMPLING_25HZ;
    pbl_accel_lock(ACCEL);
    pbl_accel_report_sample(ACCEL, &sample);
    pbl_accel_unlock(ACCEL);
  }
}

static AccelManagerState *prv_subscribe_on(PebbleTask task) {
  s_now_us = 0;
  stub_pebble_tasks_set_current(task);
  return sys_accel_manager_data_subscribe(ACCEL_SAMPLING_25HZ, prv_noop_sample_handler, nullptr,
                                          task);
}

static AccelManagerState *prv_subscribe_on_new_timers(void) {
  return prv_subscribe_on(PebbleTask_NewTimers);
}

// Consumes the full batch in a subscription's buffer and returns its timestamp
static uint64_t prv_consume_timestamp(AccelManagerState *state) {
  uint64_t timestamp_ms;
  uint32_t generation;
  bool more;
  uint32_t num_samples = sys_accel_manager_get_num_samples(state, &timestamp_ms, &generation);
  sys_accel_manager_consume_samples(state, num_samples, generation, &more);
  return timestamp_ms;
}

// Consumes the full batch in a subscription's buffer and returns whether the next one is in
static bool prv_consume(AccelManagerState *state) {
  uint64_t timestamp_ms;
  uint32_t generation;
  bool more;
  uint32_t num_samples = sys_accel_manager_get_num_samples(state, &timestamp_ms, &generation);
  sys_accel_manager_consume_samples(state, num_samples, generation, &more);
  return more;
}

//! A subscriber with no buffer, or a batch size of 0, has its data dropped. Left in the shared
//! buffer, it would keep the buffer from emptying, and every other subscriber's timestamps would
//! wrap after 65 s
void test_accel_manager__a_subscriber_without_a_buffer_doesnt_hold_back_timestamps(void) {
  prv_subscribe_on_new_timers();
  AccelManagerState *idle = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(idle, nullptr, 0);
  AccelRawData buffer[1];
  AccelManagerState *state = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(state, buffer, 1);
  const int num_samples = 70 * ACCEL_SAMPLING_25HZ;

  uint64_t timestamp_ms = 0;
  for (int i = 0; i < num_samples; i++) {
    prv_feed(1);
    timestamp_ms = prv_consume_timestamp(state);
  }

  cl_assert_equal_i(timestamp_ms, (num_samples - 1) * (1000 / ACCEL_SAMPLING_25HZ));
}

//! A buffer change while an event is out posts no second event for the subscription
void test_accel_manager__a_buffer_change_with_an_event_out_posts_no_second_event(void) {
  AccelRawData buffer[2];
  AccelRawData new_buffer[2];
  AccelManagerState *state = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  prv_feed(2);
  sys_accel_manager_set_sample_buffer(state, new_buffer, 2);

  prv_feed(2);

  cl_assert_equal_i(s_posted_events, 1);
}

//! A consume from an old buffer's event keeps the new buffer's samples
void test_accel_manager__a_consume_with_an_old_generation_keeps_the_new_samples(void) {
  AccelRawData buffer[2];
  AccelRawData new_buffer[2];
  AccelManagerState *state = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  prv_feed(2);
  uint64_t timestamp_ms;
  uint32_t old_generation;
  sys_accel_manager_get_num_samples(state, &timestamp_ms, &old_generation);
  sys_accel_manager_set_sample_buffer(state, new_buffer, 2);
  prv_feed(2);

  bool more;
  bool result = sys_accel_manager_consume_samples(state, 2, old_generation, &more);

  cl_assert(result);
  uint32_t generation;
  uint32_t num_samples = sys_accel_manager_get_num_samples(state, &timestamp_ms, &generation);
  cl_assert_equal_i(num_samples, 2);
}

//! A consume of 0 releases the event and keeps the samples, so the next full batch posts
void test_accel_manager__a_consume_of_0_releases_the_event_and_keeps_the_samples(void) {
  AccelRawData buffer[3];
  AccelRawData new_buffer[3];
  AccelManagerState *state = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(state, buffer, 3);
  prv_feed(3);
  sys_accel_manager_set_sample_buffer(state, new_buffer, 3);
  prv_feed(1);
  uint64_t timestamp_ms;
  uint32_t generation;
  bool more;
  sys_accel_manager_get_num_samples(state, &timestamp_ms, &generation);
  sys_accel_manager_consume_samples(state, 0, generation, &more);

  prv_feed(2);

  cl_assert_equal_i(s_posted_events, 2);
}

//! A batch already waiting goes to the running callback, without waiting on free queue space
void test_accel_manager__a_consume_hands_a_waiting_batch_to_the_running_callback(void) {
  AccelRawData buffer[2];
  AccelManagerState *state = prv_subscribe_on(PebbleTask_KernelBackground);
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  prv_feed(4);

  bool result = prv_consume(state);

  cl_assert(result);
  cl_assert_equal_i(fake_system_task_count_callbacks(), 1);
}

//! While the running callback has a batch, the driver posts no second event for it
void test_accel_manager__a_waiting_batch_holds_off_the_next_event(void) {
  AccelRawData buffer[2];
  AccelManagerState *state = prv_subscribe_on(PebbleTask_KernelBackground);
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  prv_feed(4);
  prv_consume(state);

  prv_feed(2);

  cl_assert_equal_i(fake_system_task_count_callbacks(), 1);
}

//! Once nothing is waiting, the consume releases the event and the next full batch posts one
void test_accel_manager__a_consume_with_nothing_waiting_releases_the_event(void) {
  AccelRawData buffer[2];
  AccelManagerState *state = prv_subscribe_on(PebbleTask_KernelBackground);
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  prv_feed(4);
  prv_consume(state);
  bool result = prv_consume(state);
  cl_assert(!result);

  prv_feed(2);

  cl_assert_equal_i(fake_system_task_count_callbacks(), 2);
}

//! A consume posts no other subscriber's event, since that could wait on their queue under the lock
void test_accel_manager__a_consume_posts_no_other_subscribers_event(void) {
  AccelRawData buffer[2];
  AccelRawData other_buffer[2];
  AccelManagerState *other = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(other, other_buffer, 2);
  AccelManagerState *state = prv_subscribe_on(PebbleTask_KernelBackground);
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  s_new_timer_queue_full = true;
  prv_feed(2);
  s_new_timer_queue_full = false;

  prv_consume(state);

  cl_assert_equal_i(s_posted_events, 0);
}

//! A NewTimers subscriber gets a waiting batch straight away too, not at the next driver batch
void test_accel_manager__a_consume_on_new_timers_hands_over_a_waiting_batch(void) {
  AccelRawData buffer[2];
  AccelManagerState *state = prv_subscribe_on_new_timers();
  sys_accel_manager_set_sample_buffer(state, buffer, 2);
  prv_feed(4);

  bool result = prv_consume(state);

  cl_assert(result);
  cl_assert_equal_i(s_posted_events, 1);
}
