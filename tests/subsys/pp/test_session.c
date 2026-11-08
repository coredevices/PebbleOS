/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/kernel/malloc.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/pp/transport.h>

#include <clar.h>
#include <kernel/event_loop.h>
#include <kernel/events.h>

// Fakes
///////////////////////////////////////////////////////////

#include <fake_kernel_malloc.h>
#include <fake_sys_work.h>

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  pbl_sys_work_call(callback, data);
}

static bool s_in_launcher_task;
bool launcher_task_is_current_task(void) {
  return s_in_launcher_task;
}

static int s_version_request_count;
void pbl_pp__version_request(struct pbl_pp_session *session) {
  ++s_version_request_count;
}

void pbl_pp__receive_drop(struct pbl_pp_session *session) {
}

static int s_system_events_open;
static int s_system_events_closed;
static int s_app_events_open;
static int s_app_events_closed;

void event_put(PebbleEvent *event) {
  if (event->type != PEBBLE_COMM_SESSION_EVENT) {
    return;
  }
  const bool is_open = event->bluetooth.comm_session_event.is_open;
  if (event->bluetooth.comm_session_event.is_system) {
    is_open ? ++s_system_events_open : ++s_system_events_closed;
  } else {
    is_open ? ++s_app_events_open : ++s_app_events_closed;
  }
}

// Listener
///////////////////////////////////////////////////////////

static int s_listener_events[3];
static bool s_listener_saw_open_session;

static void prv_listener(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                         void *user_data) {
  ++s_listener_events[event];
  s_listener_saw_open_session = (pbl_pp_session_get_type(session) != PBL_PP_SESSION_INVALID);
}

PBL_PP_SESSION_LISTENER_DEFINE(test, prv_listener, nullptr);

// Transports
///////////////////////////////////////////////////////////

static int s_send_next_count;
static int s_close_count;
static struct pbl_pp_session *s_last_closed;
static int s_reset_count;

static void prv_send_next(struct pbl_pp_session *session) {
  ++s_send_next_count;
}

static void prv_close(struct pbl_pp_session *session) {
  ++s_close_count;
  s_last_closed = session;
  pbl_pp_session_close(session);
}

static void prv_reset(struct pbl_pp_session *session) {
  ++s_reset_count;
}

static int s_responsiveness_count;
static void prv_set_responsiveness(struct pbl_pp_session *session, enum pbl_bt_consumer consumer,
                                   enum pbl_bt_response_time_state state, uint16_t max_period_secs,
                                   pbl_bt_responsiveness_granted_cb_t granted) {
  ++s_responsiveness_count;
}

static const struct pbl_pp_transport s_gatt = {
  .name = "gatt",
  .priority = 2,
  .send_next = prv_send_next,
  .close = prv_close,
  .reset = prv_reset,
  .set_responsiveness = prv_set_responsiveness,
};

static const struct pbl_pp_transport s_console = {
  .name = "console",
  .priority = 1,
  .shared = true,
  .send_next = prv_send_next,
  .close = prv_close,
};

static const struct pbl_pp_transport s_qemu = {
  .name = "qemu",
  .priority = 0,
  .send_next = prv_send_next,
};

static int s_granted_count;
static void prv_granted(void) {
  ++s_granted_count;
}

// Tests
///////////////////////////////////////////////////////////

void test_session__initialize(void) {
  fake_kernel_malloc_init();
  fake_kernel_malloc_mark();
  s_in_launcher_task = false;
  s_version_request_count = 0;
  s_system_events_open = 0;
  s_system_events_closed = 0;
  s_app_events_open = 0;
  s_app_events_closed = 0;
  memset(s_listener_events, 0, sizeof(s_listener_events));
  s_listener_saw_open_session = false;
  s_send_next_count = 0;
  s_close_count = 0;
  s_last_closed = nullptr;
  s_reset_count = 0;
  s_responsiveness_count = 0;
  s_granted_count = 0;
}

void test_session__cleanup(void) {
  fake_sys_work_callbacks_cleanup();
  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();
}

void test_session__disconnected_lookups_return_null(void) {
  const Uuid uuid = {0x04, 0xc5, 0x24, 0x01, 0x4d, 0xbe, 0x40, 0x8b,
                     0xb7, 0x3a, 0x0e, 0x80, 0xef, 0x09, 0xaf, 0x74};

  cl_assert_equal_p(pbl_pp_session_get_system(), nullptr);
  cl_assert_equal_p(pbl_pp_session_get_app(&uuid), nullptr);
  cl_assert_equal_i(pbl_pp_session_get_type(nullptr), PBL_PP_SESSION_INVALID);
}

void test_session__basic_open_close(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  cl_assert(session);
  cl_assert_equal_i(pbl_pp_session_get_type(session), PBL_PP_SESSION_SYSTEM);
  cl_assert_equal_i(s_version_request_count, 1);
  cl_assert_equal_i(s_listener_events[PBL_PP_SESSION_OPENED], 1);

  pbl_pp_session_close(session);
  cl_assert_equal_i(s_listener_events[PBL_PP_SESSION_CLOSED], 1);
  cl_assert_equal_b(s_listener_saw_open_session, true);
  cl_assert_equal_p(pbl_pp_session_get_system(), nullptr);
}

void test_session__system_events(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  cl_assert_equal_b(pbl_pp_session_is_system(session), true);
  cl_assert_equal_p(pbl_pp_session_get_system(), session);
  pbl_pp_session_close(session);

  cl_assert_equal_i(s_system_events_open, 1);
  cl_assert_equal_i(s_system_events_closed, 1);
  cl_assert_equal_i(s_app_events_open, 0);
  cl_assert_equal_i(s_app_events_closed, 0);
}

void test_session__app_events(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_APP, nullptr);
  cl_assert_equal_i(pbl_pp_session_get_type(session), PBL_PP_SESSION_APP);
  cl_assert_equal_b(pbl_pp_session_is_system(session), false);
  cl_assert_equal_p(pbl_pp_session_get_system(), nullptr);
  pbl_pp_session_close(session);

  cl_assert_equal_i(s_system_events_open, 0);
  cl_assert_equal_i(s_app_events_open, 1);
  cl_assert_equal_i(s_app_events_closed, 1);
}

void test_session__hybrid_events(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_HYBRID, nullptr);
  cl_assert_equal_b(pbl_pp_session_is_system(session), true);
  pbl_pp_session_close(session);

  cl_assert_equal_i(s_system_events_open, 1);
  cl_assert_equal_i(s_system_events_closed, 1);
  cl_assert_equal_i(s_app_events_open, 1);
  cl_assert_equal_i(s_app_events_closed, 1);
}

void test_session__last_system_session_wins(void) {
  struct pbl_pp_session *first = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  cl_assert_equal_i(s_close_count, 0);

  struct pbl_pp_session *second = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  cl_assert(second);
  cl_assert_equal_i(s_close_count, 1);
  cl_assert_equal_p(s_last_closed, first);
  cl_assert_equal_p(pbl_pp_session_get_system(), second);

  pbl_pp_session_close(second);
}

void test_session__shared_transport_does_not_close(void) {
  struct pbl_pp_session *gatt = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  struct pbl_pp_session *console =
      pbl_pp_session_open(&s_console, PBL_PP_DESTINATION_HYBRID, nullptr);
  cl_assert(console);
  cl_assert_equal_i(s_close_count, 0);

  // A non-shared session opened after a shared one leaves it alone too.
  pbl_pp_session_close(gatt);
  gatt = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  cl_assert(gatt);
  cl_assert_equal_i(s_close_count, 0);

  pbl_pp_session_close(gatt);
  pbl_pp_session_close(console);
}

void test_session__open_fails_if_system_session_cannot_be_closed(void) {
  struct pbl_pp_session *qemu = pbl_pp_session_open(&s_qemu, PBL_PP_DESTINATION_HYBRID, nullptr);
  cl_assert(qemu);

  cl_assert_equal_p(pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr), nullptr);
  cl_assert_equal_p(pbl_pp_session_get_system(), qemu);

  // App sessions do not compete for the system role.
  struct pbl_pp_session *app = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_APP, nullptr);
  cl_assert(app);

  pbl_pp_session_close(app);
  pbl_pp_session_close(qemu);
}

void test_session__system_session_follows_priority(void) {
  struct pbl_pp_session *qemu = pbl_pp_session_open(&s_qemu, PBL_PP_DESTINATION_HYBRID, nullptr);
  struct pbl_pp_session *console =
      pbl_pp_session_open(&s_console, PBL_PP_DESTINATION_HYBRID, nullptr);
  cl_assert_equal_p(pbl_pp_session_get_system(), console);

  pbl_pp_session_close(console);
  cl_assert_equal_p(pbl_pp_session_get_system(), qemu);

  pbl_pp_session_close(qemu);
}

void test_session__get_app_session(void) {
  const Uuid invalid_uuid = UUID_INVALID;
  const Uuid system_uuid = UUID_SYSTEM;
  const Uuid legacy_uuid = {0xff, 0xc5, 0x24, 0x01, 0x4d, 0xbe, 0x40, 0x8b,
                            0xb7, 0x3a, 0x0e, 0x80, 0xef, 0x09, 0xaf, 0x74};
  const Uuid modern_uuid = {0x04, 0xc5, 0x24, 0x01, 0x4d, 0xbe, 0x40, 0x8b,
                            0xb7, 0x3a, 0x0e, 0x80, 0xef, 0x09, 0xaf, 0x74};

  struct pbl_pp_session *system = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  // A transport that does not know the app UUID.
  struct pbl_pp_session *legacy = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_APP, nullptr);
  struct pbl_pp_session *modern =
      pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_APP, &modern_uuid);

  cl_assert_equal_p(pbl_pp_session_get_app(&legacy_uuid), legacy);
  cl_assert_equal_p(pbl_pp_session_get_app(&modern_uuid), modern);
  cl_assert_equal_p(pbl_pp_session_get_app(&invalid_uuid), nullptr);
  cl_assert_equal_p(pbl_pp_session_get_app(&system_uuid), nullptr);
  cl_assert_equal_p(pbl_pp_session_get_app(nullptr), nullptr);

  pbl_pp_session_close(system);
  pbl_pp_session_close(legacy);
  pbl_pp_session_close(modern);
}

void test_session__get_app_session_falls_back_to_hybrid(void) {
  const Uuid uuid = {0x04, 0xc5, 0x24, 0x01, 0x4d, 0xbe, 0x40, 0x8b,
                     0xb7, 0x3a, 0x0e, 0x80, 0xef, 0x09, 0xaf, 0x74};

  struct pbl_pp_session *hybrid = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_HYBRID, nullptr);
  cl_assert_equal_p(pbl_pp_session_get_app(&uuid), hybrid);
  pbl_pp_session_close(hybrid);
}

void test_session__send_next_deduping(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  fake_sys_work_callbacks_cleanup();

  for (int i = 0; i < 5; ++i) {
    pbl_pp_session_send_next(session);
    cl_assert_equal_i(fake_sys_work_count_callbacks(), 1);
  }
  cl_assert_equal_b(pbl_pp__send_next_is_pending(session), true);
  cl_assert_equal_i(s_send_next_count, 0);

  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(fake_sys_work_count_callbacks(), 0);
  cl_assert_equal_i(s_send_next_count, 1);
  cl_assert_equal_b(pbl_pp__send_next_is_pending(session), false);

  pbl_pp_session_send_next(session);
  pbl_pp_session_send_next(session);
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_send_next_count, 2);

  pbl_pp_session_close(session);
}

void test_session__send_next_not_called_when_session_closed_in_mean_time(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  fake_sys_work_callbacks_cleanup();

  pbl_pp_session_send_next(session);
  pbl_pp_session_close(session);
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_send_next_count, 0);
}

void test_session__send_next_now_keeps_pending_flag(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  fake_sys_work_callbacks_cleanup();

  pbl_pp_session_send_next(session);
  cl_assert_equal_b(pbl_pp__send_next_is_pending(session), true);

  pbl_pp__send_next_now(session);
  cl_assert_equal_i(s_send_next_count, 1);
  cl_assert_equal_b(pbl_pp__send_next_is_pending(session), true);

  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_b(pbl_pp__send_next_is_pending(session), false);
  cl_assert_equal_i(s_send_next_count, 2);

  pbl_pp_session_close(session);
}

void test_session__in_send_task(void) {
  s_in_launcher_task = true;
  cl_assert_equal_b(pbl_pp__in_send_task(), true);
  s_in_launcher_task = false;
  cl_assert_equal_b(pbl_pp__in_send_task(), false);
}

void test_session__reset(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  pbl_pp_session_reset(session);
  cl_assert_equal_i(s_reset_count, 1);
  pbl_pp_session_close(session);

  // Closed sessions and transports without reset are ignored.
  pbl_pp_session_reset(session);
  cl_assert_equal_i(s_reset_count, 1);

  session = pbl_pp_session_open(&s_qemu, PBL_PP_DESTINATION_SYSTEM, nullptr);
  pbl_pp_session_reset(session);
  cl_assert_equal_i(s_reset_count, 1);
  pbl_pp_session_close(session);
}

void test_session__responsiveness(void) {
  struct pbl_pp_session *gatt = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_APP, nullptr);
  pbl_pp_session_set_responsiveness(gatt, PBL_BT_CONSUMER_APP, PBL_BT_RESPONSE_TIME_MIN, 10,
                                    prv_granted);
  cl_assert_equal_i(s_responsiveness_count, 1);
  cl_assert_equal_i(s_granted_count, 0);

  // Transports without the notion grant at once, on KernelMain.
  struct pbl_pp_session *qemu = pbl_pp_session_open(&s_qemu, PBL_PP_DESTINATION_SYSTEM, nullptr);
  fake_sys_work_callbacks_cleanup();
  pbl_pp_session_set_responsiveness(qemu, PBL_BT_CONSUMER_APP, PBL_BT_RESPONSE_TIME_MIN, 10,
                                    prv_granted);
  cl_assert_equal_i(s_responsiveness_count, 1);
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_i(s_granted_count, 1);

  pbl_pp_session_close(gatt);
  pbl_pp_session_close(qemu);

  pbl_pp_session_set_responsiveness(gatt, PBL_BT_CONSUMER_APP, PBL_BT_RESPONSE_TIME_MIN, 10,
                                    prv_granted);
  cl_assert_equal_i(s_responsiveness_count, 1);
}

void test_session__capabilities(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);

  for (int i = 0; i < 32; ++i) {
    cl_assert_equal_b(pbl_pp_session_has_capability(session, 1U << i), false);
  }
  cl_assert_equal_i(s_listener_events[PBL_PP_SESSION_CAPABILITIES], 0);

  pbl_pp_session_set_capabilities(session, ~0U);
  cl_assert_equal_i(s_listener_events[PBL_PP_SESSION_CAPABILITIES], 1);
  cl_assert_equal_i(pbl_pp_session_get_capabilities(session), ~0U);
  for (int i = 0; i < 32; ++i) {
    cl_assert_equal_b(pbl_pp_session_has_capability(session, 1U << i), true);
  }

  pbl_pp_session_close(session);

  pbl_pp_session_set_capabilities(session, 0U);
  cl_assert_equal_i(s_listener_events[PBL_PP_SESSION_CAPABILITIES], 1);
  cl_assert_equal_i(pbl_pp_session_get_capabilities(session), 0U);
}

static int s_job_free_count;

static size_t prv_job_get_length(const struct pbl_pp_send_job *job) {
  return 1;
}

static void prv_job_free(struct pbl_pp_send_job *job) {
  ++s_job_free_count;
  pbl_free(job);
}

static const struct pbl_pp_send_job_api s_job_api = {
  .get_length = prv_job_get_length,
  .free = prv_job_free,
};

void test_session__close_drops_send_queue(void) {
  struct pbl_pp_session *session = pbl_pp_session_open(&s_gatt, PBL_PP_DESTINATION_SYSTEM, nullptr);
  s_job_free_count = 0;

  for (int i = 0; i < 2; ++i) {
    struct pbl_pp_send_job *job = pbl_malloc(sizeof(*job));
    *job = (struct pbl_pp_send_job){.api = &s_job_api};
    pbl_pp_send_job_submit(session, &job);
    cl_assert(job);
  }
  cl_assert_equal_i(pbl_pp_session_send_queue_length(session), 2);

  pbl_pp_session_close(session);
  cl_assert_equal_i(s_job_free_count, 2);
}
