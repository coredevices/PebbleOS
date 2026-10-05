/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/accel_service.h"
#include "applib/accel_service_private.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/accel_manager.h"

#include <stdbool.h>
#include <stdint.h>

// Fakes
#include "fake_pbl_malloc.h"
#include "fake_pebble_tasks.h"

// Stubs
#include "stubs_event_service_client.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

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

// Fake accel manager. A subscription is a tracked kernel allocation, and the test decides whether
// a data event was still queued when it unsubscribes.
struct AccelManagerState {
  int unused;
};

static bool s_event_outstanding;
static AccelDataReadyCallback s_data_cb;
static void *s_data_cb_context;

AccelManagerState *sys_accel_manager_data_subscribe(AccelSamplingRate rate,
                                                    AccelDataReadyCallback data_cb, void *context,
                                                    PebbleTask handler_task) {
  AccelManagerState *state = kernel_malloc_check(sizeof(AccelManagerState));
  *state = (AccelManagerState){};
  s_data_cb = data_cb;
  s_data_cb_context = context;
  return state;
}

bool sys_accel_manager_data_unsubscribe(AccelManagerState *state) {
  kernel_free(state);
  return s_event_outstanding;
}

uint32_t sys_accel_manager_get_max_samples_per_update(void) {
  return 25;
}

int sys_accel_manager_set_sampling_rate(AccelManagerState *state, AccelSamplingRate rate) {
  return 0;
}

int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update) {
  return 0;
}

uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms) {
  *timestamp_ms = 0;
  return 0;
}

bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples) {
  return true;
}

int sys_accel_manager_peek(AccelData *accel_data) {
  return 0;
}

// Runs the data event the manager queued before the unsubscribe, the way the process's event
// loop does once it drains its queue
static void prv_drain_stale_event(void) {
  s_data_cb(s_data_cb_context);
}

static void prv_data_handler(AccelData *data, uint32_t num_samples) {
}

static void prv_raw_data_handler(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
}

void test_accel_service__initialize(void) {
  accel_service_state_init(&s_app_state);
  accel_service_state_init(&s_worker_state);
  s_event_outstanding = false;
  s_data_cb = NULL;
  s_data_cb_context = NULL;
  stub_pebble_tasks_set_current(PebbleTask_App);
}

void test_accel_service__cleanup(void) {
  fake_pbl_malloc_check_net_allocs();
  fake_pbl_malloc_clear_tracking();
}

//! An app that unsubscribes with a data event still queued has to survive that event. The state
//! is part of the app's process state, so freeing it from the event faults the app.
void test_accel_service__app_unsubscribe_with_a_queued_event_keeps_its_state(void) {
  accel_data_service_subscribe(1, prv_data_handler);
  s_event_outstanding = true;
  accel_data_service_unsubscribe();

  prv_drain_stale_event();

  cl_assert(!s_app_state.deferred_free);
}

//! The worker goes through the same unsubscribe and has to stay safe too
void test_accel_service__worker_unsubscribe_with_a_queued_event_keeps_its_state(void) {
  stub_pebble_tasks_set_current(PebbleTask_Worker);
  accel_data_service_subscribe(1, prv_data_handler);
  s_event_outstanding = true;
  accel_data_service_unsubscribe();

  prv_drain_stale_event();

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

  prv_drain_stale_event();

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
