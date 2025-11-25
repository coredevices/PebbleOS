/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_message/app_message_internal.h"
#include "clar.h"
#include "services/normal/app_message/app_message_sender.h"
#include "services/common/comm_session/session_internal.h"
#include "services/common/comm_session/protocol.h"
#include "services/common/comm_session/session.h"
#include "process_management/app_install_manager.h"
#include "services/normal/app_outbox_service.h"
#include "util/math.h"
#include "util/net.h"

extern const SessionSendJobImpl s_app_message_send_job_impl;
extern void comm_session_send_queue_cleanup(CommSession *session);

// Fakes & Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stubs_analytics.h"
#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

static int s_app_install_timestamp_update_count;
void app_install_mark_prioritized(AppInstallId install_id, bool can_expire) {
  ++s_app_install_timestamp_update_count;
}

AppInstallId app_manager_get_current_app_id(void) {
  return INSTALL_ID_INVALID;
}

static PebbleProcessMd s_process_md;
const PebbleProcessMd* app_manager_get_current_app_md(void) {
  return &s_process_md;
}

static int s_consumed_count;
static AppOutboxStatus s_last_status_code;
void app_outbox_service_consume_message(AppOutboxMessage *message, AppOutboxStatus status) {
  s_last_status_code = status;
  ++s_consumed_count;
  kernel_free(message);
}

static AppOutboxMessageHandler s_outbox_message_handler;
static size_t s_service_data_size;
void app_outbox_service_register(AppOutboxServiceTag service_tag,
                                 AppOutboxMessageHandler message_handler,
                                 PebbleTask consumer_task,
                                 size_t service_data_size) {
  s_outbox_message_handler = message_handler;
  s_service_data_size = service_data_size;
}

static bool s_is_message_cancelled;
bool app_outbox_service_is_message_cancelled(AppOutboxMessage *message) {
  return s_is_message_cancelled;
}

void app_outbox_service_cleanup_all_pending_messages(void) {
  s_is_message_cancelled = true;
}

void comm_session_analytics_inc_bytes_sent(CommSession *session, uint16_t length) {
}

static CommSession s_system_session;
static CommSession *s_system_session_ptr;
CommSession *comm_session_get_system_session(void) {
  return s_system_session_ptr;
}

static CommSession s_app_session;
static CommSession *s_app_session_ptr;
CommSession *comm_session_get_current_app_session(void) {
  if (s_process_md.allow_js) {
    return comm_session_get_system_session();
  }
  return s_app_session_ptr;
}

bool comm_session_is_valid(const CommSession *session) {
  if (!session) {
    return false;
  }
  return (session == comm_session_get_current_app_session() ||
          session == comm_session_get_system_session());
}

static int s_send_next_count = 0;
void comm_session_send_next(CommSession *session) {
  ++s_send_next_count;
}

void comm_session_set_responsiveness(CommSession *session, BtConsumer consumer,
                                     ResponseTimeState state, uint16_t max_period_secs) {
}

void comm_session_sanitize_app_session(CommSession **session_in_out) {
  CommSession *permitted_session = comm_session_get_current_app_session();
  *session_in_out = ((!*session_in_out) ||
                     (*session_in_out == permitted_session)) ? permitted_session : NULL;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_send_outbox_raw_data(const uint8_t *data, size_t length) {
  AppOutboxMessage *outbox_message = kernel_zalloc(sizeof(AppOutboxMessage) + s_service_data_size);
  cl_assert(outbox_message);
  outbox_message->data = data;
  outbox_message->length = length;
  s_outbox_message_handler(outbox_message);
}

static AppMessageAppOutboxData *prv_create_and_send_outbox_message(CommSession *session,
                                                                   uint16_t endpoint_id,
                                                                   const uint8_t *payload,
                                                                   size_t payload_length) {
  const size_t outbox_data_size = sizeof(AppMessageAppOutboxData) + payload_length;
  AppMessageAppOutboxData *outbox_data = app_malloc(outbox_data_size);
  cl_assert(outbox_data);
  outbox_data->session = session;
  outbox_data->endpoint_id = endpoint_id;
  memcpy(outbox_data->payload, payload, payload_length);
  prv_send_outbox_raw_data((const uint8_t *)outbox_data, outbox_data_size);
  return outbox_data;
}

static void prv_process_send_queue(CommSession *session) {
  cl_assert(session);
  size_t length = comm_session_send_queue_get_length(session);
  if (length) {
    comm_session_send_queue_consume(session, length);
  }
}

#define assert_consumed(expected_last_status, expected_consumed_count) \
{ \
  cl_assert_equal_i(expected_last_status, s_last_status_code); \
  cl_assert_equal_i(expected_consumed_count, s_consumed_count); \
}

#define assert_not_consumed() \
  cl_assert_equal_i(0, s_consumed_count);

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

#define DISALLOWED_ENDPOINT_ID (9000)  // GetBytes
#define ALLOWED_ENDPOINT_ID (APP_MESSAGE_ENDPOINT_ID)

static const uint8_t TEST_PAYLOAD[] = {0xaa, 0xbb, 0xcc, 0xdd};
static uint8_t TEST_EXPECTED_PP_MSG[sizeof(PebbleProtocolHeader) + sizeof(TEST_PAYLOAD)];

void test_app_message_sender__initialize(void) {
  s_system_session_ptr = &s_system_session;
  s_app_session_ptr = &s_app_session;
  s_send_next_count = 0;

  s_outbox_message_handler = NULL;
  s_service_data_size = 0;

  s_is_message_cancelled = false;

  s_last_status_code = AppOutboxStatusUserRangeEnd;
  s_consumed_count = 0;

  s_app_install_timestamp_update_count = 0;

  s_process_md.allow_js = false;

  PebbleProtocolHeader *header = (PebbleProtocolHeader *)TEST_EXPECTED_PP_MSG;
  header->length = htons(sizeof(TEST_PAYLOAD));
  header->endpoint_id = htons(ALLOWED_ENDPOINT_ID);
  memcpy(TEST_EXPECTED_PP_MSG + sizeof(*header), TEST_PAYLOAD, sizeof(TEST_PAYLOAD));

  app_message_sender_init();
  cl_assert(s_outbox_message_handler);
}

void test_app_message_sender__cleanup(void) {
  // Flush out to avoid other tests failing:
  if (s_system_session_ptr) {
    prv_process_send_queue(s_system_session_ptr);
  }
  if (s_app_session_ptr) {
    prv_process_send_queue(s_app_session_ptr);
  }
}

// Tests that exercise the sanity checking of the input from the app
////////////////////////////////////////////////////////////////////////////////////////////////////

void test_app_message_sender__outbox_data_too_short(void) {
  // This is one byte too small, because the PP payload has to be at least one in length:
  AppMessageAppOutboxData data = {};
  prv_send_outbox_raw_data((const uint8_t *)&data, sizeof(data));
  assert_consumed(AppMessageSenderErrorDataTooShort, 1);
}

void test_app_message_sender__disallowed_endpoint(void) {
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(s_system_session_ptr, DISALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorEndpointDisallowed, 1);
  app_free(outbox_data);
}

void test_app_message_sender__system_session_but_not_js_app(void) {
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(s_system_session_ptr, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  app_free(outbox_data);
}

void test_app_message_sender__app_session_but_js_app(void) {
  s_process_md.allow_js = true;
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(s_app_session_ptr, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  app_free(outbox_data);
}

void test_app_message_sender__no_sessions_connected(void) {
  s_system_session_ptr = NULL;
  s_app_session_ptr = NULL;
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  app_free(outbox_data);
}

void test_app_message_sender__auto_select_not_js_app(void) {
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  prv_process_send_queue(s_system_session_ptr);
  assert_not_consumed();

  prv_process_send_queue(s_app_session_ptr);
  assert_consumed(AppMessageSenderErrorSuccess, 1);

  app_free(outbox_data);
}

void test_app_message_sender__auto_select_js_app(void) {
  s_process_md.allow_js = true;
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  prv_process_send_queue(s_app_session_ptr);
  assert_not_consumed();

  prv_process_send_queue(s_system_session_ptr);
  assert_consumed(AppMessageSenderErrorSuccess, 1);

  app_free(outbox_data);
}

void test_app_message_sender__system_session_and_js_app(void) {
  s_process_md.allow_js = true;
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(s_system_session_ptr, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_not_consumed();
  prv_process_send_queue(s_system_session_ptr);
  assert_consumed(AppMessageSenderErrorSuccess, 1);
  cl_assert_equal_i(s_app_install_timestamp_update_count, 1);
  app_free(outbox_data);
}

// Tests that exercise interface towards the Send Queue
////////////////////////////////////////////////////////////////////////////////////////////////////

void test_app_message_sender__freed_but_not_sent_entirely(void) {
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  size_t length = comm_session_send_queue_get_length(s_app_session_ptr);
  comm_session_send_queue_consume(s_app_session_ptr, length - 1);
  comm_session_send_queue_cleanup(s_app_session_ptr);
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  cl_assert_equal_i(s_app_install_timestamp_update_count, 0);
  app_free(outbox_data);
}

void test_app_message_sender__byte_by_byte_consume(void) {
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  size_t length = comm_session_send_queue_get_length(s_app_session_ptr);
  cl_assert_equal_i(length, sizeof(PebbleProtocolHeader) + sizeof(TEST_PAYLOAD));

  for (int i = 0; i < length; ++i) {
    // Test the `length` implementation:
    cl_assert_equal_i(length - i, comm_session_send_queue_get_length(s_app_session_ptr));

    // Test the `read_pointer` implementation:
    const uint8_t *read_pointer = NULL;
    size_t length_available = comm_session_send_queue_get_read_pointer(s_app_session_ptr,
                                                                       &read_pointer);
    cl_assert(read_pointer);
    cl_assert_equal_i(TEST_EXPECTED_PP_MSG[i], *read_pointer);
    // Expect that the header and payload will be non-contiguous:
    if (i < sizeof(PebbleProtocolHeader)) {
      cl_assert_equal_i(sizeof(PebbleProtocolHeader) - i, length_available);
    } else {
      cl_assert_equal_i(length - i, length_available);
    }

    // Test the `copy` implementation:
    uint8_t byte_out = 0xff;
    cl_assert_equal_i(1, comm_session_send_queue_copy(s_app_session_ptr, 0 /* offset */,
                                                      1 /* length */, &byte_out));
    cl_assert_equal_i(TEST_EXPECTED_PP_MSG[i], byte_out);

    comm_session_send_queue_consume(s_app_session_ptr, 1 /* length */);
  }

  assert_consumed(AppMessageSenderErrorSuccess, 1);
  cl_assert_equal_i(s_app_install_timestamp_update_count, 1);
  app_free(outbox_data);
}

void test_app_message_sender__byte_by_byte_copy_with_offset(void) {
  AppMessageAppOutboxData *outbox_data =
      prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                         TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  size_t length = comm_session_send_queue_get_length(s_app_session_ptr);
  cl_assert_equal_i(length, sizeof(PebbleProtocolHeader) + sizeof(TEST_PAYLOAD));

  uint8_t bytes_out[length];
  memset(bytes_out, 0xff, length);

  // Consume byte by byte:
  for (int c = 0; c < length; ++c) {
    // Shift offset byte by byte:
    for (int o = 0; o < (length - c); ++o) {
      size_t length_to_copy = (length - c - o);
      cl_assert_equal_i(length_to_copy, comm_session_send_queue_copy(s_app_session_ptr, o,
                                                                     length_to_copy, bytes_out));
      cl_assert_equal_i(0, memcmp(bytes_out, TEST_EXPECTED_PP_MSG + o + c, length_to_copy));
    }

    comm_session_send_queue_consume(s_app_session_ptr, 1 /* length */);
  }

  assert_consumed(AppMessageSenderErrorSuccess, 1);
  cl_assert_equal_i(s_app_install_timestamp_update_count, 1);
  app_free(outbox_data);
}

// Tests that deal with the edge case of app outbox messages getting cancelled,
// because the app that provides the buffer for the payload is quit while they are in the
// process of being sent out.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_quit_app_after_pp_msg_byte(uint32_t num_bytes) {
  AppMessageAppOutboxData *outbox_data =
  prv_create_and_send_outbox_message(NULL /* auto-select */, ALLOWED_ENDPOINT_ID,
                                     TEST_PAYLOAD, sizeof(TEST_PAYLOAD));

  size_t length = comm_session_send_queue_get_length(s_app_session_ptr);
  uint8_t bytes_out[length];
  memset(bytes_out, 0xff, length);

  // Copy & consume one byte of the header -- note the header is 4 bytes total:
  size_t first_length = num_bytes;
  cl_assert_equal_i(first_length, comm_session_send_queue_copy(s_app_session_ptr, 0,
                                                               first_length, bytes_out));
  comm_session_send_queue_consume(s_app_session_ptr, first_length);

  // App quits with only one header byte consumed:
  app_outbox_service_cleanup_all_pending_messages();

  // Copy & consume the rest:
  size_t second_length = (length - first_length);
  cl_assert_equal_i(second_length, comm_session_send_queue_copy(s_app_session_ptr, 0,
                                                                second_length,
                                                                bytes_out + first_length));
  comm_session_send_queue_consume(s_app_session_ptr, second_length);

  // The message should be consumed now (to free the resources associated with it):
  assert_consumed(AppMessageSenderErrorSuccess, 1);

  // Expect at least the PebbleProtocol header or more to be intact:
  size_t intact_size = MAX(num_bytes, sizeof(PebbleProtocolHeader));
  cl_assert_equal_m(bytes_out, TEST_EXPECTED_PP_MSG, intact_size);

  // Expect the remainder to be filled with zeroes:
  for (int i = 0; i < (length - intact_size); ++i) {
    cl_assert_equal_i(bytes_out[intact_size + i], 0x00);
  }

  app_free(outbox_data);
}

void test_app_message_sender__cancelled_message_in_flight_header_and_payload_not_finished(void) {
  // Expect header to get sent out normally, then a payload with all zeroes
  prv_quit_app_after_pp_msg_byte(sizeof(PebbleProtocolHeader) - 1);
}

void test_app_message_sender__cancelled_message_in_flight_payload_not_finished(void) {
  // Expect remainder payload to be all zeroes
  prv_quit_app_after_pp_msg_byte(sizeof(PebbleProtocolHeader) + 1);
}
