/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <pbl/services/accel_manager.h>

#include <applib/accel_service.h>
#include <applib/accel_service_private.h>
#include <clar.h>
#include <kernel/pebble_tasks.h>

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

// Fake accel manager. A subscription holds the samples in its buffer, the samples waiting behind
// them, whether a data event is out for it, and its buffer generation. Subscriptions come from a
// small pool that hands back the slot freed most recently, the way a heap can hand back the block
// it just freed, so a new subscription can land at the old one's address.
struct AccelManagerState {
  uint32_t num_samples;
  uint32_t waiting_samples;
  uint32_t samples_per_update;
  uint32_t generation;
  bool event_posted;
};

#define FAKE_SUBSCRIPTION_SLOTS 4
static AccelManagerState s_slots[FAKE_SUBSCRIPTION_SLOTS];
static bool s_slot_used[FAKE_SUBSCRIPTION_SLOTS];
static int s_last_freed_slot;

static bool s_refuse_buffer;
static int s_live_subscriptions;
static int s_fewest_live_subscriptions;
static AccelManagerState *s_current;
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
  s_current = state;
  s_data_cb = data_cb;
  s_data_cb_context = context;
  return state;
}

// The real manager reports an event that's out until its consume, including one running now
bool sys_accel_manager_data_unsubscribe(AccelManagerState *state) {
  prv_assert_live(state);
  s_slot_used[state - s_slots] = false;
  s_last_freed_slot = state - s_slots;
  s_live_subscriptions--;
  if (s_live_subscriptions < s_fewest_live_subscriptions) {
    s_fewest_live_subscriptions = s_live_subscriptions;
  }
  return state->event_posted;
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

// A refused buffer keeps the old one. A new one starts empty, with a new generation
int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update) {
  prv_assert_live(state);
  if (s_refuse_buffer) {
    return -1;
  }
  state->num_samples = 0;
  state->samples_per_update = samples_per_update;
  state->generation++;
  return 0;
}

uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms,
                                           uint32_t *generation) {
  prv_assert_live(state);
  *timestamp_ms = 1000;
  *generation = state->generation;
  return state->num_samples;
}

// As in the real manager, only a consume of the current generation empties the buffer. The waiting
// samples then top it up, and a full batch stays with the running event.
bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples,
                                       uint32_t generation, bool *more) {
  prv_assert_live(state);
  s_consume_calls++;
  bool success = true;
  if (generation == state->generation && samples > 0) {
    success = (samples == state->num_samples);
    state->num_samples = 0;
  }
  uint32_t room = (state->samples_per_update > state->num_samples)
                      ? state->samples_per_update - state->num_samples
                      : 0;
  uint32_t moved = (state->waiting_samples < room) ? state->waiting_samples : room;
  state->num_samples += moved;
  state->waiting_samples -= moved;
  *more = state->samples_per_update > 0 && state->num_samples >= state->samples_per_update;
  state->event_posted = *more;
  return success;
}

int sys_accel_manager_peek(AccelData *accel_data) {
  return 0;
}

// Posts a data event for the current subscription and returns its context
static void *prv_post(void) {
  s_current->event_posted = true;
  return s_data_cb_context;
}

// Fills the current subscription with samples and runs its data event
static void prv_deliver(uint32_t num_samples) {
  s_current->num_samples = num_samples;
  s_data_cb(prv_post());
}

static int s_data_handler_calls;
static int s_raw_data_handler_calls;
static uint64_t s_timestamps[2];
static AccelServiceState *s_session;

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

static void prv_resizing_handler(AccelData *data, uint32_t num_samples) {
  s_data_handler_calls++;
  accel_service_set_samples_per_update(2);
}

static int16_t s_read_after_change;

static void prv_deleting_raw_handler(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
  s_raw_data_handler_calls++;
  accel_session_data_unsubscribe(s_session);
  accel_session_delete(s_session);
  s_read_after_change = data[0].x;
}

static void prv_rebatching_raw_handler(AccelRawData *data, uint32_t num_samples,
                                       uint64_t timestamp) {
  s_raw_data_handler_calls++;
  accel_service_set_samples_per_update(2);
  s_read_after_change = data[0].x;
}

static void prv_unsubscribing_raw_handler(AccelRawData *data, uint32_t num_samples,
                                          uint64_t timestamp) {
  s_raw_data_handler_calls++;
  accel_data_service_unsubscribe();
  s_read_after_change = data[0].x;
}

static void prv_resubscribing_raw_handler(AccelRawData *data, uint32_t num_samples,
                                          uint64_t timestamp) {
  s_raw_data_handler_calls++;
  accel_raw_data_service_subscribe(1, prv_raw_data_handler);
  s_read_after_change = data[0].x;
}

static void prv_twice_rebatching_raw_handler(AccelRawData *data, uint32_t num_samples,
                                             uint64_t timestamp) {
  s_raw_data_handler_calls++;
  accel_service_set_samples_per_update(2);
  accel_service_set_samples_per_update(3);
  s_read_after_change = data[0].x;
}

static void prv_rebatching_deprecated_handler(AccelRawData *data, uint32_t num_samples) {
  accel_service_set_samples_per_update(2);
  s_read_after_change = data[0].x;
}

void test_accel_service__initialize(void) {
  accel_service_state_init(&s_app_state);
  accel_service_state_init(&s_worker_state);
  for (int i = 0; i < FAKE_SUBSCRIPTION_SLOTS; i++) {
    s_slot_used[i] = false;
  }
  s_last_freed_slot = -1;
  s_refuse_buffer = false;
  s_live_subscriptions = 0;
  s_fewest_live_subscriptions = INT_MAX;
  s_current = NULL;
  s_data_cb = NULL;
  s_data_cb_context = NULL;
  s_consume_calls = 0;
  s_data_handler_calls = 0;
  s_raw_data_handler_calls = 0;
  s_timestamps[0] = 0;
  s_timestamps[1] = 0;
  s_session = NULL;
  s_read_after_change = 0;
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

//! A data event queued before an unsubscribe is dropped without touching the app's state
void test_accel_service__a_queued_event_after_unsubscribe_calls_no_handler(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  void *stale_context = prv_post();
  accel_data_service_unsubscribe();

  s_data_cb(stale_context);

  cl_assert_equal_i(s_data_handler_calls, 0);
  cl_assert_equal_i(s_consume_calls, 0);
}

//! The same for a worker
void test_accel_service__a_queued_event_after_a_worker_unsubscribes_calls_no_handler(void) {
  stub_pebble_tasks_set_current(PebbleTask_Worker);
  accel_data_service_subscribe(1, prv_data_handler);
  void *stale_context = prv_post();
  accel_data_service_unsubscribe();

  s_data_cb(stale_context);

  cl_assert_equal_i(s_data_handler_calls, 0);
  cl_assert_equal_i(s_consume_calls, 0);
}

//! A kernel session with a queued event is freed on delete. The event only reads its token
void test_accel_service__a_kernel_session_with_a_queued_event_is_freed_on_delete(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  void *stale_context = prv_post();
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);

  s_data_cb(stale_context);

  cl_assert_equal_i(s_raw_data_handler_calls, 0);
}

//! A kernel session with nothing queued is freed on delete too
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

//! An event queued for the old subscription is dropped, even with a batch in the new one
void test_accel_service__a_stale_event_after_a_replace_calls_no_handler(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  void *stale_context = prv_post();
  accel_data_service_subscribe(1, prv_data_handler);
  s_current->num_samples = 1;

  s_data_cb(stale_context);

  cl_assert_equal_i(s_data_handler_calls, 0);
  cl_assert_equal_i(s_consume_calls, 0);
}

//! A replaced kernel session is freed on delete with an event queued for the old subscription
void test_accel_service__a_replaced_kernel_session_is_freed_on_delete(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  void *stale_context = prv_post();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);

  s_data_cb(stale_context);

  cl_assert_equal_i(s_raw_data_handler_calls, 0);
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

// Changing the batch size
//////////////////////////////////////////

//! A batch size of 0 allocates no buffer, since applib_malloc(0) returns NULL on the watch
void test_accel_service__a_batch_size_of_0_allocates_no_buffer(void) {
  accel_data_service_subscribe(1, prv_data_handler);

  int result = accel_service_set_samples_per_update(0);

  cl_assert_equal_i(result, 0);
  cl_assert(s_app_state.raw_data == NULL);
}

//! A data event at a batch size of 0 doesn't call the handler
void test_accel_service__an_event_at_batch_size_0_calls_no_handler(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  accel_service_set_samples_per_update(0);

  prv_deliver(0);

  cl_assert_equal_i(s_data_handler_calls, 0);
}

//! A buffer the manager refuses on a resubscribe leaves the old subscription and handler in place
void test_accel_service__a_refused_buffer_on_resubscribe_keeps_the_old_subscription(void) {
  accel_data_service_subscribe(1, prv_unsubscribing_handler);
  AccelManagerState *old_state = s_current;
  void *old_context = s_data_cb_context;
  s_refuse_buffer = true;
  accel_data_service_subscribe(1, prv_data_handler);
  s_refuse_buffer = false;
  s_current = old_state;
  s_data_cb_context = old_context;

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 1);
  cl_assert_equal_i(s_live_subscriptions, 0);
}

//! A refused buffer keeps the old buffer and batch size, which the manager still uses
void test_accel_service__a_refused_buffer_keeps_the_old_one(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  s_refuse_buffer = true;
  accel_service_set_samples_per_update(2);

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 1);
}

//! A handler that changes its batch size releases its event, rather than failing the consume
void test_accel_service__changing_the_batch_size_in_the_handler_keeps_the_app_running(void) {
  accel_data_service_subscribe(1, prv_resizing_handler);

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 1);
  cl_assert(!s_current->event_posted);
}

//! An event queued before a batch size change releases, so the next batch still posts
void test_accel_service__a_batch_size_change_with_an_event_queued_keeps_data_coming(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  void *queued_context = prv_post();
  accel_service_set_samples_per_update(2);

  s_data_cb(queued_context);

  cl_assert_equal_i(s_data_handler_calls, 0);
  cl_assert(!s_current->event_posted);
}

//! An event queued before a batch size change delivers the new buffer if it's already full
void test_accel_service__a_queued_event_delivers_a_full_new_buffer(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  void *queued_context = prv_post();
  accel_service_set_samples_per_update(2);
  s_current->num_samples = 2;

  s_data_cb(queued_context);

  cl_assert_equal_i(s_data_handler_calls, 1);
  cl_assert_equal_i(s_current->num_samples, 0);
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

//! A batch already waiting is handled in the same event, so a backlog never waits on queue space
void test_accel_service__an_event_handles_a_waiting_batch_in_the_same_pass(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  s_current->waiting_samples = 1;

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 2);
  cl_assert(!s_current->event_posted);
}

//! An event that finds a short buffer delivers it in the same pass once the consume tops it up
void test_accel_service__a_short_buffer_topped_up_is_delivered_in_the_same_pass(void) {
  accel_data_service_subscribe(2, prv_data_handler);
  s_current->num_samples = 1;
  s_current->waiting_samples = 1;

  s_data_cb(prv_post());

  cl_assert_equal_i(s_data_handler_calls, 1);
  cl_assert(!s_current->event_posted);
}

//! A handler that changes its batch size gets the full new batch waiting behind it in the same pass
void test_accel_service__a_resized_buffer_waiting_is_delivered_in_the_same_pass(void) {
  accel_data_service_subscribe(1, prv_resizing_handler);
  s_current->waiting_samples = 2;

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 2);
  cl_assert(!s_current->event_posted);
}

//! A handler that unsubscribes stops the pass, even with another batch waiting
void test_accel_service__unsubscribing_in_the_handler_stops_the_pass(void) {
  accel_data_service_subscribe(1, prv_unsubscribing_handler);
  s_current->waiting_samples = 1;

  prv_deliver(1);

  cl_assert_equal_i(s_data_handler_calls, 1);
}

//! A kernel session deleted in its own handler is freed at once
void test_accel_service__a_kernel_session_deleted_in_its_handler_is_freed_at_once(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  s_session = accel_session_create();
  accel_session_raw_data_subscribe(s_session, ACCEL_SAMPLING_25HZ, 1, prv_deleting_raw_handler);

  prv_deliver(1);

  cl_assert_equal_i(s_raw_data_handler_calls, 1);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! A raw handler can still read its buffer after changing its batch size
void test_accel_service__a_raw_handler_keeps_its_buffer_after_changing_its_batch_size(void) {
  accel_raw_data_service_subscribe(1, prv_rebatching_raw_handler);
  s_app_state.raw_data[0].x = 7;

  prv_deliver(1);

  cl_assert_equal_i(s_read_after_change, 7);
}

//! The same when the raw handler unsubscribes
void test_accel_service__a_raw_handler_keeps_its_buffer_after_unsubscribing(void) {
  accel_raw_data_service_subscribe(1, prv_unsubscribing_raw_handler);
  s_app_state.raw_data[0].x = 7;

  prv_deliver(1);

  cl_assert_equal_i(s_read_after_change, 7);
}

//! The same when the raw handler subscribes again, which replaces the subscription and its buffer
void test_accel_service__a_raw_handler_keeps_its_buffer_after_subscribing_again(void) {
  accel_raw_data_service_subscribe(1, prv_resubscribing_raw_handler);
  s_app_state.raw_data[0].x = 7;

  prv_deliver(1);

  cl_assert_equal_i(s_read_after_change, 7);
}

//! The same after two changes in one handler
void test_accel_service__a_raw_handler_keeps_its_buffer_after_two_changes(void) {
  accel_raw_data_service_subscribe(1, prv_twice_rebatching_raw_handler);
  s_app_state.raw_data[0].x = 7;

  prv_deliver(1);

  cl_assert_equal_i(s_read_after_change, 7);
}

//! The deprecated raw handler is handed the buffer itself too
void test_accel_service__a_deprecated_raw_handler_keeps_its_buffer_after_a_change(void) {
  accel_data_service_subscribe__deprecated(1, prv_rebatching_deprecated_handler);
  s_app_state.raw_data[0].x = 7;

  prv_deliver(1);

  cl_assert_equal_i(s_read_after_change, 7);
}

//! The same when a kernel session's raw handler deletes the session
void test_accel_service__a_raw_handler_keeps_its_buffer_after_deleting_its_session(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  s_session = accel_session_create();
  accel_session_raw_data_subscribe(s_session, ACCEL_SAMPLING_25HZ, 1, prv_deleting_raw_handler);
  s_session->raw_data[0].x = 7;

  prv_deliver(1);

  cl_assert_equal_i(s_read_after_change, 7);
}

// Which task changes a kernel session
//////////////////////////////////////////

//! Unsubscribing a kernel session off its handler task asserts
void test_accel_service__unsubscribing_from_another_task_asserts(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  cl_assert_passert(accel_session_data_unsubscribe(session));

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
}

//! An unsubscribed session can be deleted from any task, since a queued event only reads its token
void test_accel_service__an_unsubscribed_session_can_be_deleted_from_another_task(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  void *stale_context = prv_post();
  accel_session_data_unsubscribe(session);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  accel_session_delete(session);

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  s_data_cb(stale_context);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

//! An unsubscribed session can subscribe again on another task
void test_accel_service__an_unsubscribed_session_can_subscribe_on_another_task(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  accel_session_data_unsubscribe(session);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  prv_deliver(1);

  cl_assert_equal_i(s_raw_data_handler_calls, 1);
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
}

//! Subscribing a kernel session again off its handler task asserts
void test_accel_service__subscribing_again_from_another_task_asserts(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  cl_assert_passert(
      accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler));

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
}

//! The same for changing its batch size
void test_accel_service__changing_the_batch_size_from_another_task_asserts(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  cl_assert_passert(accel_session_set_samples_per_update(session, 2));

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
}

//! The same for changing its sampling rate
void test_accel_service__changing_the_sampling_rate_from_another_task_asserts(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  accel_session_raw_data_subscribe(session, ACCEL_SAMPLING_25HZ, 1, prv_raw_data_handler);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  cl_assert_passert(accel_session_set_sampling_rate(session, ACCEL_SAMPLING_50HZ));

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  accel_session_data_unsubscribe(session);
  accel_session_delete(session);
}

//! A session that never subscribed, like a shake-only one, can be deleted from any task
void test_accel_service__a_session_that_never_subscribed_can_be_deleted_anywhere(void) {
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  AccelServiceState *session = accel_session_create();
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);

  accel_session_delete(session);

  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}
