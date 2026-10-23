/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_pebble_tasks.h"
#include "stubs_kernel_syscall.h"

#include <pbl/services/event_service.h>
#include <pbl/util/size.h>

#include <applib/event_service_client.h>
#include <clar.h>
#include <kernel/events.h>
#include <kernel/memory_layout.h>
#include <process_management/process_manager.h>
#include <syscall/syscall.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fakes

bool arch_syscall_returns_to_user(void *ret_addr) {
  return true;
}

static PebbleEvent s_posted_event;
static int s_num_posted_events;

bool event_try_put_from_process(PebbleTask task, PebbleEvent *event) {
  s_posted_event = *event;
  s_num_posted_events++;
  return true;
}

static bool s_text_in_region;

bool memory_layout_is_cstring_in_region(const MpuRegion *region, const char *str,
                                        size_t max_length) {
  return s_text_in_region;
}

static int16_t s_plugin_index;

int16_t sys_event_service_get_plugin_service_index(const Uuid *uuid) {
  cl_assert_equal_p(uuid, nullptr);
  return s_plugin_index;
}

static ProcessContext s_process_context;

ProcessContext *app_manager_get_task_context(void) {
  return &s_process_context;
}

ProcessContext *worker_manager_get_task_context(void) {
  return &s_process_context;
}

static EventServiceInfo s_event_service_state;

EventServiceInfo *app_state_get_event_service_state(void) {
  return &s_event_service_state;
}

EventServiceInfo *worker_state_get_event_service_state(void) {
  return &s_event_service_state;
}

bool event_service_filter(ListNode *node, void *tp) {
  return false;
}

struct pbl_msgq *event_kernel_to_kernel_event_queue(void) {
  return nullptr;
}

void event_service_subscribe_from_kernel_main(PebbleSubscriptionEvent *subscription) {
}

uint32_t process_manager_process_events_waiting(PebbleTask task) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

static PebbleTaskBitset prv_all_but(PebbleTaskBitset tasks) {
  return (PebbleTaskBitset)~tasks;
}

static void prv_post(PebbleEvent event) {
  sys_send_pebble_event_to_kernel(&event);
}

static void prv_subscribe(PebbleEventType type) {
  EventServiceInfo info = {.type = type};
  sys_event_service_client_subscribe(&info);
}

static void prv_unsubscribe(PebbleEventType type) {
  EventServiceInfo info = {.type = type};
  sys_event_service_client_unsubscribe(&s_event_service_state, &info);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

void test_event_syscalls__initialize(void) {
  stubs_syscall_init();
  stub_pebble_tasks_set_current(PebbleTask_App);
  s_posted_event = (PebbleEvent){};
  s_num_posted_events = 0;
  s_text_in_region = true;
  s_plugin_index = 1;
  s_event_service_state = (EventServiceInfo){};
}

void test_event_syscalls__subscribe_allowed(void) {
  prv_subscribe(PEBBLE_TICK_EVENT);
  cl_assert_equal_i(s_num_posted_events, 1);
  cl_assert_equal_i(s_posted_event.type, PEBBLE_SUBSCRIPTION_EVENT);
  cl_assert_equal_b(s_posted_event.subscription.subscribe, true);
  cl_assert_equal_i(s_posted_event.subscription.task, PebbleTask_App);
  cl_assert_equal_i(s_posted_event.subscription.event_type, PEBBLE_TICK_EVENT);
}

void test_event_syscalls__subscribe_rejected(void) {
  const PebbleEventType types[] = {
    PEBBLE_SYS_NOTIFICATION_EVENT, PBL_BT_PEBBLE_PAIRING_EVENT, PEBBLE_PUT_BYTES_EVENT,
    PEBBLE_BLOBDB_EVENT,           PEBBLE_PROCESS_KILL_EVENT,   PEBBLE_NUM_EVENTS,
  };
  for (size_t i = 0; i < ARRAY_LENGTH(types); i++) {
    stubs_syscall_init();
    cl_assert_panic(prv_subscribe(types[i]));
    assert_syscall_failed();
  }
  cl_assert_equal_i(s_num_posted_events, 0);
}

void test_event_syscalls__unsubscribe_rejected(void) {
  cl_assert_panic(prv_unsubscribe(PEBBLE_SYS_NOTIFICATION_EVENT));
  assert_syscall_failed();
  cl_assert_equal_i(s_num_posted_events, 0);
}

void test_event_syscalls__post_rejected_type(void) {
  cl_assert_panic(prv_post((PebbleEvent){.type = PEBBLE_CALLBACK_EVENT}));
  assert_syscall_failed();
  cl_assert_equal_i(s_num_posted_events, 0);
}

void test_event_syscalls__post_kill(void) {
  stub_pebble_tasks_set_current(PebbleTask_Worker);
  prv_post((PebbleEvent){
    .type = PEBBLE_PROCESS_KILL_EVENT,
    .task_mask = (1 << PebbleTask_KernelMain),
    .kill = {.task = PebbleTask_App, .gracefully = true},
  });
  cl_assert_equal_i(s_num_posted_events, 1);
  cl_assert_equal_i(s_posted_event.kill.task, PebbleTask_Worker);
  cl_assert_equal_b(s_posted_event.kill.gracefully, true);
  cl_assert_equal_i(s_posted_event.task_mask, prv_all_but(1 << PebbleTask_KernelMain));
}

void test_event_syscalls__post_render_ready(void) {
  prv_post((PebbleEvent){.type = PEBBLE_RENDER_READY_EVENT});
  cl_assert_equal_i(s_num_posted_events, 1);
  cl_assert_equal_i(s_posted_event.task_mask, prv_all_but(1 << PebbleTask_KernelMain));

  stub_pebble_tasks_set_current(PebbleTask_Worker);
  cl_assert_panic(prv_post((PebbleEvent){.type = PEBBLE_RENDER_READY_EVENT}));
  assert_syscall_failed();
  cl_assert_equal_i(s_num_posted_events, 1);
}

void test_event_syscalls__post_dictation(void) {
  prv_post((PebbleEvent){
    .type = PEBBLE_DICTATION_EVENT,
    .dictation = {.text = "hello"},
  });
  cl_assert_equal_i(s_num_posted_events, 1);
  cl_assert_equal_i(s_posted_event.task_mask, prv_all_but(1 << PebbleTask_App));

  prv_post((PebbleEvent){.type = PEBBLE_DICTATION_EVENT});
  cl_assert_equal_i(s_num_posted_events, 2);

  s_text_in_region = false;
  cl_assert_panic(prv_post((PebbleEvent){
    .type = PEBBLE_DICTATION_EVENT,
    .dictation = {.text = "hello"},
  }));
  assert_syscall_failed();
  cl_assert_equal_i(s_num_posted_events, 2);
}

void test_event_syscalls__post_health(void) {
  stub_pebble_tasks_set_current(PebbleTask_Worker);
  prv_post((PebbleEvent){
    .type = PEBBLE_HEALTH_SERVICE_EVENT,
    .health_event = {
      .type = HealthEventSignificantUpdate,
      .data.significant_update.day_id = 3,
    },
  });
  cl_assert_equal_i(s_num_posted_events, 1);
  cl_assert_equal_i(s_posted_event.health_event.type, HealthEventSignificantUpdate);
  cl_assert_equal_i(s_posted_event.health_event.data.significant_update.day_id, 0);
  cl_assert_equal_i(s_posted_event.task_mask, prv_all_but(1 << PebbleTask_Worker));

  cl_assert_panic(prv_post((PebbleEvent){
    .type = PEBBLE_HEALTH_SERVICE_EVENT,
    .health_event = {.type = HealthEventHeartRateUpdate},
  }));
  assert_syscall_failed();
  cl_assert_equal_i(s_num_posted_events, 1);
}

void test_event_syscalls__post_plugin(void) {
  s_plugin_index = 7;
  prv_post((PebbleEvent){
    .type = PEBBLE_PLUGIN_SERVICE_EVENT,
    .task_mask = (1 << PebbleTask_Worker),
    .plugin_service = {.service_index = 3, .type = 5},
  });
  cl_assert_equal_i(s_num_posted_events, 1);
  cl_assert_equal_i(s_posted_event.plugin_service.service_index, 7);
  cl_assert_equal_i(s_posted_event.plugin_service.type, 5);
  cl_assert_equal_i(s_posted_event.task_mask,
                    prv_all_but((1 << PebbleTask_App) | (1 << PebbleTask_Worker)));

  s_plugin_index = -1;
  prv_post((PebbleEvent){.type = PEBBLE_PLUGIN_SERVICE_EVENT});
  cl_assert_equal_i(s_num_posted_events, 2);
  cl_assert_equal_i(s_posted_event.task_mask, prv_all_but(0));
}
