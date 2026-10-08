/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include <applib/accel_service.h>
#include <applib/accel_service_private.h>
#include <kernel/pebble_tasks.h>
#include <pbl/services/accel_manager.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

// Fakes
#include <fake_pbl_malloc.h>
#include <fake_pebble_tasks.h>

// Stubs
#include <stubs_event_service_client.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

bool sys_vibe_history_was_vibrating(uint64_t time_search) {
  return false;
}

int32_t sys_vibe_get_vibe_strength(void) {
  return 0;
}

// The app and worker states live inside their process state, not on any heap
static AccelServiceState s_app_state;
static AccelServiceState s_worker_state;

AccelServiceState *app_state_get_accel_state(void) {
  return &s_app_state;
}

AccelServiceState *worker_state_get_accel_state(void) {
  return &s_worker_state;
}

// Fake accel manager. A subscription holds the samples waiting for its subscriber, and the test
// decides whether a data event was still queued when it unsubscribes. Subscriptions come from a
// small pool that hands back the slot freed most recently, the way a heap can hand back the block
// it just freed, so a new subscription can land at the old one's address.
struct AccelManagerState {
  uint32_t num_samples;
};

#define FAKE_SUBSCRIPTION_SLOTS 4
static AccelManagerState s_slots[FAKE_SUBSCRIPTION_SLOTS];
static bool s_slot_used[FAKE_SUBSCRIPTION_SLOTS];
static int s_last_freed_slot;

static bool s_event_outstanding;
static int s_live_subscriptions;
static int s_fewest_live_subscriptions;
static AccelDataReadyCallback s_data_cb;
static void *s_data_cb_context;
static int s_consume_calls;

// The real manager kills an app that passes a subscription it doesn't know
static void prv_assert_live(const AccelManagerState *state) {
  cl_assert(state >= s_slots && state < s_slots + FAKE_SUBSCRIPTION_SLOTS);
  cl_assert(s_slot_used[state - s_slots]);
}

AccelManagerState *sys_accel_manager_data_subscribe(AccelSamplingRate rate,
                                                    AccelDataReadyCallback data_cb, void *context,
                                                    PebbleTask handler_task) {
  int slot = s_last_freed_slot;
  if (slot < 0 || s_slot_used[slot]) {
    slot = 0;
    while (s_slot_used[slot]) {
      slot++;
      cl_assert(slot < FAKE_SUBSCRIPTION_SLOTS);
    }
  }
  s_slot_used[slot] = true;
  s_last_freed_slot = -1;
  AccelManagerState *state = &s_slots[slot];
  *state = (AccelManagerState){};
  s_live_subscriptions++;
  s_data_cb = data_cb;
  s_data_cb_context = context;
  return state;
}

bool sys_accel_manager_data_unsubscribe(AccelManagerState *state) {
  prv_assert_live(state);
  s_slot_used[state - s_slots] = false;
  s_last_freed_slot = state - s_slots;
  s_live_subscriptions--;
  if (s_live_subscriptions < s_fewest_live_subscriptions) {
    s_fewest_live_subscriptions = s_live_subscriptions;
  }
  return s_event_outstanding;
}

uint32_t sys_accel_manager_get_max_samples_per_update(void) {
  return 25;
}

// The real manager only accepts the four supported rates
int sys_accel_manager_set_sampling_rate(AccelManagerState *state, AccelSamplingRate rate) {
  prv_assert_live(state);
  switch (rate) {
    case ACCEL_SAMPLING_10HZ:
    case ACCEL_SAMPLING_25HZ:
    case ACCEL_SAMPLING_50HZ:
    case ACCEL_SAMPLING_100HZ:
      return 0;
    default:
      return -1;
  }
}

int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update) {
  prv_assert_live(state);
  return 0;
}

uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms) {
  prv_assert_live(state);
  *timestamp_ms = 1000;
  return state->num_samples;
}

// The real manager also reports a failure when asked to consume a different number of samples
// than the subscription holds
bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples) {
  prv_assert_live(state);
  s_consume_calls++;
  bool success = (samples == state->num_samples);
  state->num_samples = 0;
  return success;
}

int sys_accel_manager_peek(AccelData *accel_data) {
  return 0;
}

// Fills the app's current subscription with samples and runs its data event
static void prv_deliver(uint32_t num_samples) {
  s_app_state.manager_state->num_samples = num_samples;
  s_data_cb(s_data_cb_context);
}

static int s_data_handler_calls;
static int s_raw_data_handler_calls;
static uint64_t s_timestamps[2];

static void prv_data_handler(AccelData *data, uint32_t num_samples) {
  s_data_handler_calls++;
  for (uint32_t i = 0; i < num_samples && i < 2; i++) {
    s_timestamps[i] = data[i].timestamp;
  }
}

static void prv_raw_data_handler(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
  s_raw_data_handler_calls++;
}

static void prv_unsubscribing_handler(AccelData *data, uint32_t num_samples) {
  s_data_handler_calls++;
  accel_data_service_unsubscribe();
}

static void prv_resubscribing_handler(AccelData *data, uint32_t num_samples) {
  s_data_handler_calls++;
  accel_data_service_unsubscribe();
  accel_data_service_subscribe(1, prv_data_handler);
}

static void prv_replacing_handler(AccelData *data, uint32_t num_samples) {
  s_data_handler_calls++;
  accel_data_service_subscribe(1, prv_data_handler);
}

void test_accel_service__initialize(void) {
  accel_service_state_init(&s_app_state);
  accel_service_state_init(&s_worker_state);
  for (int i = 0; i < FAKE_SUBSCRIPTION_SLOTS; i++) {
    s_slot_used[i] = false;
  }
  s_last_freed_slot = -1;
  s_event_outstanding = false;
  s_live_subscriptions = 0;
  s_fewest_live_subscriptions = INT_MAX;
  s_data_cb = NULL;
  s_data_cb_context = NULL;
  s_consume_calls = 0;
  s_data_handler_calls = 0;
  s_raw_data_handler_calls = 0;
  s_timestamps[0] = 0;
  s_timestamps[1] = 0;
  stub_pebble_tasks_set_current(PebbleTask_App);
}

void test_accel_service__cleanup(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);
  accel_data_service_unsubscribe();
  cl_assert_equal_i(s_live_subscriptions, 0);
  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
}

// Unsubscribing with a data event still queued
//////////////////////////////////////////

//! An app that unsubscribes with a data event still queued has to survive that event. The state
//! is part of the app's process state, so freeing it from the event faults the app.
void test_accel_service__app_unsubscribe_with_a_queued_event_keeps_its_state(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  s_event_outstanding = true;
  accel_data_service_unsubscribe();

  s_data_cb(s_data_cb_context);

  cl_assert(!s_app_state.deferred_free);
}

//! The worker goes through the same unsubscribe and has to stay safe too
void test_accel_service__worker_unsubscribe_with_a_queued_event_keeps_its_state(void) {
  stub_pebble_tasks_set_current(PebbleTask_Worker);
  accel_data_service_subscribe(1, prv_data_handler);
  s_event_outstanding = true;
  accel_data_service_unsubscribe();

  s_data_cb(s_data_cb_context);

  cl_assert(!s_worker_state.deferred_free);
}

//! A kernel session deleted with a data event still queued is freed by that event, not before
void test_accel_service__kernel_session_with_a_queued_event_is_freed_by_the_event(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  s_event_outstanding = true;
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 1);

  s_data_cb(s_data_cb_context);

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! A kernel session with nothing queued is freed straight away by accel_session_delete()
void test_accel_service__kernel_session_without_a_queued_event_is_freed_on_delete(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  accel_session_data_unsubscribe(session);

  accel_session_delete(session);

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

// Subscribing again
//////////////////////////////////////////

//! Subscribing twice replaces the first subscription. Left registered, the manager keeps writing
//! into the first sample buffer after it's freed and keeps the accelerometer running.
void test_accel_service__subscribing_again_replaces_the_subscription(void) {
  accel_data_service_subscribe(1, prv_data_handler);

  accel_data_service_subscribe(5, prv_data_handler);

  cl_assert_equal_i(s_live_subscriptions, 1);
}

//! The same when switching from processed to raw samples
void test_accel_service__switching_to_raw_data_replaces_the_subscription(void) {
  accel_data_service_subscribe(1, prv_data_handler);

  accel_raw_data_service_subscribe(1, prv_raw_data_handler);

  cl_assert_equal_i(s_live_subscriptions, 1);
}

//! The new subscription is added before the old one goes. With none in between, the manager
//! clears the vibe history and drops the driver to its idle rate.
void test_accel_service__a_replace_never_leaves_the_manager_without_a_subscriber(void) {
  accel_data_service_subscribe(1, prv_data_handler);

  accel_data_service_subscribe(5, prv_data_handler);

  cl_assert_equal_i(s_fewest_live_subscriptions, 1);
}

//! After a replace, samples go to the new handler only
void test_accel_service__a_replaced_subscription_delivers_to_the_new_handler(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  accel_raw_data_service_subscribe(1, prv_raw_data_handler);

  prv_deliver(1);

  cl_assert_equal_i(s_raw_data_handler_calls, 1);
  cl_assert_equal_i(s_data_handler_calls, 0);
  cl_assert_equal_i(s_consume_calls, 1);
}

//! A data event queued for the old subscription runs against the new one, and delivers only what
//! the new subscription holds
void test_accel_service__a_stale_event_after_a_replace_uses_the_new_subscription(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  AccelDataReadyCallback stale_cb = s_data_cb;
  void *stale_context = s_data_cb_context;
  s_event_outstanding = true;
  accel_data_service_subscribe(1, prv_data_handler);
  s_event_outstanding = false;

  stale_cb(stale_context);

  cl_assert_equal_i(s_data_handler_calls, 0);
  cl_assert_equal_i(s_consume_calls, 0);
}

//! A kernel session that subscribes again with an event queued for the old subscription, then
//! unsubscribes and is deleted, is freed by that event. Freed on delete, the event runs on freed
//! memory.
void test_accel_service__a_replaced_kernel_session_is_freed_by_the_stale_event(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  s_event_outstanding = true;
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  s_event_outstanding = false;
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 1);

  s_data_cb(s_data_cb_context);

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! Subscribing records its own rate. A rate left over from the old subscription spaces the
//! sample timestamps for the wrong rate.
void test_accel_service__subscribing_again_resets_the_sampling_rate(void) {
  accel_data_service_subscribe(2, prv_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
  accel_data_service_subscribe(2, prv_data_handler);

  prv_deliver(2);

  cl_assert_equal_i(s_timestamps[1] - s_timestamps[0], 40);
}

//! A rate the manager turns down isn't kept. The samples still arrive at the old rate, so spacing
//! their timestamps for the rejected one gets them wrong.
void test_accel_service__a_rejected_sampling_rate_is_not_kept(void) {
  accel_data_service_subscribe(2, prv_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
  accel_service_set_sampling_rate((AccelSamplingRate)30);

  prv_deliver(2);

  cl_assert_equal_i(s_timestamps[1] - s_timestamps[0], 10);
}

// Changing the subscription inside the data handler
//////////////////////////////////////////

//! A handler that unsubscribes leaves nothing to consume. Consuming for the subscription it just
//! dropped gets the app killed.
void test_accel_service__unsubscribing_in_the_handler_skips_the_consume(void) {
  accel_data_service_subscribe(1, prv_unsubscribing_handler);

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 1);
  cl_assert_equal_i(s_consume_calls, 0);
}

//! A handler that unsubscribes and subscribes again has a new, empty subscription, which then
//! delivers normally. It can land at the old one's address, so the check can't compare pointers.
void test_accel_service__unsubscribing_and_subscribing_in_the_handler_skips_the_consume(void) {
  accel_data_service_subscribe(1, prv_resubscribing_handler);
  prv_deliver(1);
  cl_assert_equal_i(s_consume_calls, 0);

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 2);
  cl_assert_equal_i(s_consume_calls, 1);
}

//! A handler that subscribes again without unsubscribing replaces its subscription the same way
void test_accel_service__subscribing_again_in_the_handler_skips_the_consume(void) {
  accel_data_service_subscribe(1, prv_replacing_handler);
  prv_deliver(1);
  cl_assert_equal_i(s_consume_calls, 0);

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 2);
  cl_assert_equal_i(s_consume_calls, 1);
}

//! A handler that keeps its subscription still consumes what it was given
void test_accel_service__a_normal_handler_consumes_its_samples(void) {
  accel_data_service_subscribe(1, prv_data_handler);

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 1);
  cl_assert_equal_i(s_consume_calls, 1);
}
