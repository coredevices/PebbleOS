/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_inbox.h"
#include "applib/app_message/app_message_internal.h"
#include "applib/app_message/app_message_receiver.h"
#include "comm/bt_conn_mgr.h"
#include "services/common/comm_session/session_receive_router.h"
#include "kernel/events.h"
#include "process_management/app_install_types.h"

extern const ReceiverImplementation g_app_message_receiver_implementation;

static const ReceiverImplementation *s_rcv_imp = &g_app_message_receiver_implementation;

#define MAX_HEADER_SIZE (sizeof(AppMessageHeader))
#define BUFFER_SIZE (sizeof(AppMessagePush))

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fakes & Stubs

#include "fake_kernel_malloc.h"
#include "fake_system_task.h"

#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_syscall_internal.h"

bool process_manager_send_event_to_process(PebbleTask task, PebbleEvent* e) {
  cl_assert_equal_i(PEBBLE_CALLBACK_EVENT, e->type);

  // Use fake_system_task as mock implementation:
  system_task_add_callback(e->callback.callback, e->callback.data);
  return true;
}

static void prv_process_events(void) {
  fake_system_task_callbacks_invoke_pending();
}

static AppInbox *s_app_message_inbox;
AppInbox **app_state_get_app_message_inbox(void) {
  return &s_app_message_inbox;
}

static bool s_communication_timestamp_updated;
void app_install_mark_prioritized(AppInstallId install_id, bool can_expire) {
  s_communication_timestamp_updated = true;
}

AppInstallId app_manager_get_current_app_id(void) {
  return INSTALL_ID_INVALID;
}

static uint8_t s_app_message_pp_buffer[BUFFER_SIZE];
static size_t s_app_message_pp_received_length;

void app_message_app_protocol_msg_callback(CommSession *session,
                                           const uint8_t* data, size_t length,
                                           AppInboxConsumerInfo *consumer_info) {
  cl_assert(length <= BUFFER_SIZE);
  memcpy(s_app_message_pp_buffer, data, length);
  s_app_message_pp_received_length = length;
}

static void prv_protocol_msg_callback(CommSession *session,
                                      const uint8_t* data, size_t length) {
  app_message_app_protocol_msg_callback(session, data, length, NULL);
}

void app_message_inbox_handle_dropped_messages(uint32_t num_drops) {
}

void comm_session_set_responsiveness(CommSession *session, BtConsumer consumer,
                                     ResponseTimeState state, uint16_t max_period_secs) {
}

static bool s_kernel_receiver_available;
static Receiver *s_kernel_receiver;
static bool s_kernel_receiver_is_receiving;
static uint8_t s_kernel_receiver_buffer[MAX_HEADER_SIZE];
static off_t s_kernel_receiver_buffer_idx;
static bool s_kernel_receiver_finish_called;
static bool s_kernel_receiver_cleanup_called;

static Receiver *prv_default_kernel_receiver_prepare(CommSession *session,
                                                     const PebbleProtocolEndpoint *endpoint,
                                                     size_t total_payload_size) {
  if (!s_kernel_receiver_available) {
    return NULL;
  }
  s_kernel_receiver_is_receiving = true;
  cl_assert_equal_p(endpoint->handler, app_message_app_protocol_system_nack_callback);
  cl_assert(total_payload_size <= MAX_HEADER_SIZE);
  return s_kernel_receiver;
}

static void prv_default_kernel_receiver_write(Receiver *receiver,
                                              const uint8_t *data, size_t length) {
  cl_assert_equal_p(receiver, s_kernel_receiver);
  memcpy(s_kernel_receiver_buffer + s_kernel_receiver_buffer_idx, data, length);
  s_kernel_receiver_buffer_idx += length;
  cl_assert(s_kernel_receiver_buffer_idx <= MAX_HEADER_SIZE);
}

static void prv_default_kernel_receiver_finish(Receiver *receiver) {
  cl_assert_equal_p(receiver, s_kernel_receiver);
  s_kernel_receiver_is_receiving = false;
  s_kernel_receiver_finish_called = true;
}

static void prv_default_kernel_receiver_cleanup(Receiver *receiver) {
  cl_assert_equal_p(receiver, s_kernel_receiver);
  s_kernel_receiver_is_receiving = false;
  s_kernel_receiver_cleanup_called = true;
}

const ReceiverImplementation g_default_kernel_receiver_implementation = {
  .prepare = prv_default_kernel_receiver_prepare,
  .write = prv_default_kernel_receiver_write,
  .finish = prv_default_kernel_receiver_finish,
  .cleanup = prv_default_kernel_receiver_cleanup,
};

void app_message_app_protocol_system_nack_callback(CommSession *session,
                                                   const uint8_t* data, size_t length) {
}
void test_dropped_handler(uint32_t num_dropped_messages) {}
void test_message_handler(const uint8_t *data, size_t length,
                          AppInboxConsumerInfo *consumer_info) {}
void test_alt_message_handler(const uint8_t *data, size_t length,
                              AppInboxConsumerInfo *consumer_info) {}
void test_alt_dropped_handler(uint32_t num_dropped_messages) {}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

static CommSession *s_session = (CommSession *)0xaabbccdd;

void test_app_message_receiver__initialize(void) {
  s_kernel_receiver_available = true;
  s_kernel_receiver = (Receiver *)0xffaaffaa;
  s_kernel_receiver_is_receiving = false;
  s_kernel_receiver_finish_called = false;
  s_kernel_receiver_cleanup_called = false;
  s_kernel_receiver_buffer_idx = 0;
  memset(s_kernel_receiver_buffer, 0, sizeof(s_kernel_receiver_buffer));
  s_app_message_pp_received_length = 0;
  memset(s_app_message_pp_buffer, 0, sizeof(s_app_message_pp_buffer));
  fake_kernel_malloc_init();
  fake_kernel_malloc_enable_stats(true);
  s_communication_timestamp_updated = false;
}

void test_app_message_receiver__cleanup(void) {
  fake_system_task_callbacks_cleanup();
  fake_kernel_malloc_deinit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Forwarding to default system receiver to nack the message

static const AppMessagePush s_push = {
  .header = {
    .command = CMD_PUSH,
    .transaction_id = 0xa5,
  },
};

static const PebbleProtocolEndpoint s_app_message_endpoint = (const PebbleProtocolEndpoint) {
  .endpoint_id = APP_MESSAGE_ENDPOINT_ID,
  .handler = prv_protocol_msg_callback,
  .access_mask = PebbleProtocolAccessAny,
  .receiver_imp = &g_app_message_receiver_implementation,
  .receiver_opt = NULL,
};

void test_app_message_receiver__receive_push_but_inbox_not_opened(void) {
  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(AppMessagePush));
  cl_assert(r != NULL);
  cl_assert_equal_b(true, s_kernel_receiver_is_receiving);

  s_rcv_imp->write(r, (const uint8_t *)&s_push, sizeof(s_push));

  // Expect only up to MAX_HEADER_SIZE bytes has been written:
  cl_assert_equal_i(s_kernel_receiver_buffer_idx, MAX_HEADER_SIZE);
  // Check that the header is received correctly:
  cl_assert_equal_m(s_kernel_receiver_buffer, &s_push, MAX_HEADER_SIZE);

  s_rcv_imp->finish(r);
  prv_process_events();
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
  cl_assert_equal_b(true, s_kernel_receiver_finish_called);
}

void test_app_message_receiver__receive_push_but_inbox_not_opened_then_cleanup(void) {
  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(AppMessagePush));
  s_rcv_imp->write(r, (const uint8_t *)&s_push, sizeof(s_push));

  s_rcv_imp->cleanup(r);
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
  cl_assert_equal_b(true, s_kernel_receiver_cleanup_called);
}

void test_app_message_receiver__receive_push_but_inbox_not_opened_kernel_oom(void) {
  fake_kernel_malloc_set_largest_free_block(0);

  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(AppMessagePush));
  cl_assert_equal_p(r, NULL);
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
}

void test_app_message_receiver__receive_push_but_inbox_not_opened_no_kernel_receiver(void) {
  fake_kernel_malloc_mark();
  s_kernel_receiver_available = false;
  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(AppMessagePush));
  cl_assert_equal_p(r, NULL);
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
  fake_kernel_malloc_mark_assert_equal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Normal flow: writing message to app message inbox

static Receiver *prv_create_inbox_prepare_and_write(void) {
  cl_assert_equal_b(true, app_message_receiver_open(sizeof(AppMessagePush)));
  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(AppMessagePush));
  cl_assert(r != NULL);

  s_rcv_imp->write(r, (const uint8_t *)&s_push, sizeof(s_push));
  return r;
}

static void prv_destroy_inbox(void) {
  app_message_receiver_close();
}

void test_app_message_receiver__receive_push(void) {
  Receiver *r = prv_create_inbox_prepare_and_write();

  s_rcv_imp->finish(r);
  prv_process_events();
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
  cl_assert_equal_b(false, s_kernel_receiver_finish_called);

  cl_assert_equal_m(&s_push, s_app_message_pp_buffer, sizeof(s_push));
  cl_assert_equal_i(s_app_message_pp_received_length, sizeof(s_push));

  cl_assert_equal_b(true, s_communication_timestamp_updated);

  prv_destroy_inbox();
}

void test_app_message_receiver__receive_push_then_cleanup(void) {
  Receiver *r = prv_create_inbox_prepare_and_write();

  s_rcv_imp->cleanup(r);
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
  cl_assert_equal_b(false, s_kernel_receiver_finish_called);

  cl_assert_equal_i(s_app_message_pp_received_length, 0);

  cl_assert_equal_b(true, s_communication_timestamp_updated);

  prv_destroy_inbox();
}

void test_app_message_receiver__receive_push_buffer_overflow(void) {
  cl_assert_equal_b(true, app_message_receiver_open(sizeof(AppMessagePush)));
  // Write an ACK, we should be able to fit one (N)ACK in addition to the Push message:
  AppMessageAck ack = {};
  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(ack));
  cl_assert(r != NULL);
  s_rcv_imp->write(r, (const uint8_t *)&ack, sizeof(ack));
  s_rcv_imp->finish(r);

  cl_assert_equal_b(false, s_kernel_receiver_finish_called);
  cl_assert_equal_b(true, s_kernel_receiver_cleanup_called);
  s_app_message_pp_received_length = 0;
  s_kernel_receiver_buffer_idx = 0;

  // Write a Push:
  r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                         sizeof(AppMessagePush));
  cl_assert(r != NULL);
  s_rcv_imp->write(r, (const uint8_t *)&s_push, sizeof(s_push));

  // Write some more, doesn't fit in the buffer:
  s_rcv_imp->write(r, (const uint8_t *)&s_push, sizeof(s_push));
  s_rcv_imp->finish(r);

  prv_process_events();

  // Header fwd to default system receiver should have finished, so it can be nacked:
  cl_assert_equal_b(true, s_kernel_receiver_finish_called);

  // Only Ack is received:
  cl_assert_equal_i(s_app_message_pp_received_length, sizeof(AppMessageAck));
  prv_destroy_inbox();
}

// Covers race as described here: PBL-41464
// 1. A part of a big app message is being received, causing it to get received in chunks.
//    It's not received completely yet.
// 2. app_message_outbox_closed is called.
// 3. app_message_outbox_open is called, resetting the receiver state.
// 4. Next chunk comes in. We used to assert: the "writer" (receiver state) was not NULL.
//    Fix: just eat the message and fail the app message by letting the KernelReceiver NACK it.
void test_app_message_receiver__receive_multi_chunk_push_while_open_close_toggle(void) {
  cl_assert_equal_b(true, app_message_receiver_open(sizeof(AppMessagePush)));

  Receiver *r = s_rcv_imp->prepare(s_session, &s_app_message_endpoint,
                                   sizeof(AppMessagePush));
  cl_assert(r != NULL);

  // Receive only first byte of the push message:
  s_rcv_imp->write(r, (const uint8_t *)&s_push, 1);

  // Close app message and open again:
  app_message_receiver_close();
  cl_assert_equal_b(true, app_message_receiver_open(sizeof(AppMessagePush)));

  // Receive the remainder of the push message:
  s_rcv_imp->write(r, ((const uint8_t *)&s_push) + 1, sizeof(s_push) - 1);
  s_rcv_imp->finish(r);

  // Header fwd to default system receiver should have finished, so it can be nacked:
  prv_process_events();
  cl_assert_equal_b(false, s_kernel_receiver_is_receiving);
  cl_assert_equal_b(true, s_kernel_receiver_finish_called);

  prv_destroy_inbox();
}
