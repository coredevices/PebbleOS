/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_outbox.h"
#include "applib/event_service_client.h"
#include "services/normal/app_outbox_service.h"
#include "clar.h"

extern void app_outbox_service_deinit(void);
extern uint32_t app_outbox_service_max_pending_messages(AppOutboxServiceTag tag);
extern uint32_t app_outbox_service_max_message_length(AppOutboxServiceTag tag);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fakes & Stubs

#include "fake_kernel_malloc.h"
#include "fake_pebble_tasks.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_syscall_internal.h"

static EventServiceInfo s_app_state_app_outbox_subscription_info;
EventServiceInfo *app_state_get_app_outbox_subscription_info(void) {
  return &s_app_state_app_outbox_subscription_info;
}

void event_service_client_subscribe(EventServiceInfo * service_info) {
}

void sys_send_pebble_event_to_kernel(PebbleEvent* event) {
  cl_assert_equal_i(event->type, PEBBLE_APP_OUTBOX_MSG_EVENT);
  event->callback.callback(event->callback.data);
}

static int s_num_app_outbox_events_sent;
bool process_manager_send_event_to_process(PebbleTask task, PebbleEvent* e) {
  cl_assert_equal_i(e->type, PEBBLE_APP_OUTBOX_SENT_EVENT);
  cl_assert(e->app_outbox_sent.sent_handler);
  e->app_outbox_sent.sent_handler(e->app_outbox_sent.status,
                                  e->app_outbox_sent.cb_ctx);
  ++s_num_app_outbox_events_sent;
  return true;
}

void app_message_outbox_handle_app_outbox_message_sent(AppOutboxStatus status, void *cb_ctx) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

static int s_num_message_handler_calls;
static AppOutboxMessage *s_last_message;
static void prv_message_handler(AppOutboxMessage *message) {
  s_last_message = message;
  ++s_num_message_handler_calls;
}

static int s_num_sent_handler_called;
static AppOutboxStatus s_last_sent_status;
static void *s_expected_cb_ctx = (void *)0x77777777;
void test_app_outbox_sent_handler(AppOutboxStatus status, void *cb_ctx) {
  cl_assert_equal_p(s_expected_cb_ctx, cb_ctx);
  s_last_sent_status = status;
  ++s_num_sent_handler_called;
}

#define assert_sent_cb_last_status(expected_status) \
    cl_assert_equal_i(s_last_sent_status, expected_status)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

static uint8_t *s_test_data;
static size_t s_test_data_length;

void test_app_outbox__initialize(void) {
  fake_kernel_malloc_init();
  fake_kernel_malloc_enable_stats(true);

  s_test_data_length = app_outbox_service_max_message_length(AppOutboxServiceTagUnitTest);
  s_test_data = kernel_malloc(s_test_data_length);
  memset(s_test_data, 0x88, s_test_data_length);

  s_num_sent_handler_called = 0;
  s_num_app_outbox_events_sent = 0;
  s_num_message_handler_calls = 0;
  s_last_message = NULL;

  stubs_syscall_init();
  s_app_state_app_outbox_subscription_info = (EventServiceInfo) {};
  // set to something that is not expected anywhere in the tests:
  s_last_sent_status = AppOutboxStatusUserRangeEnd;

  app_outbox_service_init();
  app_outbox_init();
}

void test_app_outbox__cleanup(void) {
  app_outbox_service_deinit();

  kernel_free(s_test_data);
}

static size_t s_consumer_data_length = 1;

static void prv_register(void) {
  app_outbox_service_register(AppOutboxServiceTagUnitTest,
                              prv_message_handler,
                              PebbleTask_KernelMain,
                              s_consumer_data_length);
}

void test_app_outbox__register_twice_asserts(void) {
  prv_register();
  cl_assert_passert(prv_register());
}

void test_app_outbox__send_not_user_space_buffer(void) {
  // TODO: really implement privilege escalation in unit tests. See PBL-9688
  return;
  cl_assert_passert(app_outbox_send(NULL, s_test_data_length,
                                    test_app_outbox_sent_handler, s_expected_cb_ctx));
  assert_syscall_failed();
}

// Disallowed, because it's not white-listed in app_outbox_service.c
static void prv_disallowed_sent_handler(AppOutboxStatus status, void *cb_ctx) {
}

void test_app_outbox__send_disallowed_sent_handler(void) {
  prv_register();
  cl_assert_passert(app_outbox_send(s_test_data, s_test_data_length,
                                    prv_disallowed_sent_handler, s_expected_cb_ctx));
  assert_syscall_failed();
}

void test_app_outbox__send_max_length_exceeded(void) {
  prv_register();
  cl_assert_passert(app_outbox_send(s_test_data, s_test_data_length + 1,
                                    test_app_outbox_sent_handler, s_expected_cb_ctx));
  assert_syscall_failed();
}

void test_app_outbox__send_but_consumer_not_registered(void) {
  prv_register();
  app_outbox_service_unregister(AppOutboxServiceTagUnitTest);

  app_outbox_send(s_test_data, s_test_data_length,
                  test_app_outbox_sent_handler, s_expected_cb_ctx);
  assert_sent_cb_last_status(AppOutboxStatusConsumerDoesNotExist);
}

void test_app_outbox__send_but_max_pending_messages_reached(void) {
  prv_register();

  uint32_t max_pending_messages =
      app_outbox_service_max_pending_messages(AppOutboxServiceTagUnitTest);

  for (uint32_t i = 0; i < max_pending_messages; ++i) {
    app_outbox_send(s_test_data, s_test_data_length,
                    test_app_outbox_sent_handler, s_expected_cb_ctx);
    cl_assert_equal_i(s_num_sent_handler_called, 0);
  }

  app_outbox_send(s_test_data, s_test_data_length,
                  test_app_outbox_sent_handler, s_expected_cb_ctx);
  assert_sent_cb_last_status(AppOutboxStatusOutOfResources);
}

void test_app_outbox__send_but_oom(void) {
  prv_register();
  fake_kernel_malloc_set_largest_free_block(0);
  app_outbox_send(s_test_data, s_test_data_length,
                  test_app_outbox_sent_handler, s_expected_cb_ctx);
  assert_sent_cb_last_status(AppOutboxStatusOutOfMemory);
}

void test_app_outbox__send_but_null_sent_handler(void) {
  prv_register();
  // Invalid data, so normally an event would get put to invoke the sent_handler,
  // but sent handler is NULL. Expect no events to be put.
  cl_assert_passert(app_outbox_send(NULL, 0, NULL, s_expected_cb_ctx));
  cl_assert_equal_i(s_num_app_outbox_events_sent, 0);
}

void test_app_outbox__send(void) {
  fake_kernel_malloc_mark();

  prv_register();

  uint32_t max_pending_messages =
      app_outbox_service_max_pending_messages(AppOutboxServiceTagUnitTest);

  AppOutboxMessage *message[max_pending_messages];
  for (uint32_t i = 0; i < max_pending_messages; ++i) {
    app_outbox_send(s_test_data, s_test_data_length,
                    test_app_outbox_sent_handler, s_expected_cb_ctx);
    cl_assert_equal_i(s_num_app_outbox_events_sent, 0);
    cl_assert_equal_i(s_num_message_handler_calls, i + 1);
    cl_assert(s_last_message);
    cl_assert_equal_p(s_test_data, s_last_message->data);
    cl_assert_equal_i(s_test_data_length, s_last_message->length);

    cl_assert_equal_b(false, app_outbox_service_is_message_cancelled(s_last_message));

    message[i] = s_last_message;
  }

  for (uint32_t i = 0; i < max_pending_messages; ++i) {
    app_outbox_service_consume_message(message[i], AppOutboxStatusSuccess);
    cl_assert_equal_i(s_num_app_outbox_events_sent, i + 1);
    assert_sent_cb_last_status(AppOutboxStatusSuccess);
  }

  fake_kernel_malloc_mark_assert_equal();
}

void test_app_outbox__unregister_with_pending_message(void) {
  fake_kernel_malloc_mark();

  prv_register();
  app_outbox_send(s_test_data, s_test_data_length,
                  test_app_outbox_sent_handler, s_expected_cb_ctx);
  cl_assert(s_last_message);

  app_outbox_service_unregister(AppOutboxServiceTagUnitTest);
  cl_assert_equal_i(s_num_app_outbox_events_sent, 1);
  assert_sent_cb_last_status(AppOutboxStatusConsumerDoesNotExist);

  cl_assert_equal_b(true, app_outbox_service_is_message_cancelled(s_last_message));
  // the consumer must call ..._consume_message(), to free the resources:
  app_outbox_service_consume_message(s_last_message, AppOutboxStatusSuccess);

  // sent_handler shouldn't get called again, it's already been called:
  cl_assert_equal_i(s_num_app_outbox_events_sent, 1);

  fake_kernel_malloc_mark_assert_equal();
}

void test_app_outbox__cleanup_all_with_pending_message(void) {
  fake_kernel_malloc_mark();

  prv_register();
  app_outbox_send(s_test_data, s_test_data_length,
                  test_app_outbox_sent_handler, s_expected_cb_ctx);
  cl_assert(s_last_message);

  app_outbox_service_cleanup_all_pending_messages();

  // sent_handler shouldn't get called when cleaning up:
  cl_assert_equal_i(s_num_app_outbox_events_sent, 0);

  cl_assert_equal_b(true, app_outbox_service_is_message_cancelled(s_last_message));
  // the consumer must call ..._consume_message(), to free the resources:
  app_outbox_service_consume_message(s_last_message, AppOutboxStatusSuccess);

  fake_kernel_malloc_mark_assert_equal();
}

