/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/malloc.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/services/app_message/app_message_sender.h>
#include <pbl/services/app_outbox_service.h>
#include <pbl/services/app_session.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#include <applib/app_malloc.h>
#include <applib/app_message/app_message_internal.h>
#include <clar.h>
#include <fff.h>
#include <pp/pp_private.h>
#include <process_management/app_install_manager.h>

extern const struct pbl_pp_send_job_api s_app_message_send_job_impl;

// Fakes & Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

FAKE_VOID_FUNC(app_install_mark_prioritized, AppInstallId, bool);

AppInstallId app_manager_get_current_app_id(void) {
  return INSTALL_ID_INVALID;
}

static PebbleProcessMd s_process_md;
const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return &s_process_md;
}

FAKE_VOID_FUNC(app_outbox_service_consume_message, AppOutboxMessage *, AppOutboxStatus);
static void prv_consume_message(AppOutboxMessage *message, AppOutboxStatus status) {
  pbl_free(message);
}

FAKE_VOID_FUNC(app_outbox_service_register, AppOutboxServiceTag, AppOutboxMessageHandler,
               PebbleTask, size_t);

static bool s_is_message_cancelled;
bool app_outbox_service_is_message_cancelled(AppOutboxMessage *message) {
  return s_is_message_cancelled;
}

void app_outbox_service_cleanup_all_pending_messages(void) {
  s_is_message_cancelled = true;
}

static struct pbl_pp_session s_system_session;
static struct pbl_pp_session *s_system_session_ptr;
struct pbl_pp_session *pbl_pp_session_get_system(void) {
  return s_system_session_ptr;
}

static struct pbl_pp_session s_app_session;
static struct pbl_pp_session *s_app_session_ptr;
struct pbl_pp_session *app_session_get_current(void) {
  if (s_process_md.allow_js) {
    return pbl_pp_session_get_system();
  }
  return s_app_session_ptr;
}

void pbl_pp__lock(void) {
}

void pbl_pp__unlock(void) {
}

void pbl_pp__assert_locked(void) {
}

bool pbl_pp__session_is_open(const struct pbl_pp_session *session) {
  if (!session) {
    return false;
  }
  return (session == app_session_get_current() || session == pbl_pp_session_get_system());
}

FAKE_VOID_FUNC(pbl_pp_session_send_next, struct pbl_pp_session *);

void pbl_pp_session_set_responsiveness(struct pbl_pp_session *session,
                                       enum pbl_bt_consumer consumer,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs,
                                       pbl_bt_responsiveness_granted_cb_t granted) {
}

void app_session_sanitize(struct pbl_pp_session **session_in_out) {
  struct pbl_pp_session *permitted_session = app_session_get_current();
  *session_in_out =
      ((!*session_in_out) || (*session_in_out == permitted_session)) ? permitted_session : nullptr;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_send_outbox_raw_data(const uint8_t *data, size_t length) {
  AppOutboxMessage *outbox_message =
      pbl_zalloc(sizeof(AppOutboxMessage) + app_outbox_service_register_fake.arg3_val);
  cl_assert(outbox_message);
  outbox_message->data = data;
  outbox_message->length = length;
  app_outbox_service_register_fake.arg1_val(outbox_message);
}

static AppMessageAppOutboxData *prv_create_and_send_outbox_message(struct pbl_pp_session *session,
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

static void prv_process_send_queue(struct pbl_pp_session *session) {
  cl_assert(session);
  size_t length = pbl_pp_session_send_queue_length(session);
  if (length) {
    pbl_pp_session_send_queue_consume(session, length);
  }
}

#define assert_consumed(expected_last_status, expected_consumed_count)                         \
  {                                                                                            \
    cl_assert_equal_i(expected_last_status, app_outbox_service_consume_message_fake.arg1_val); \
    cl_assert_equal_i(expected_consumed_count,                                                 \
                      app_outbox_service_consume_message_fake.call_count);                     \
  }

#define assert_not_consumed() \
  cl_assert_equal_i(0, app_outbox_service_consume_message_fake.call_count);

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

#define DISALLOWED_ENDPOINT_ID (9000) // GetBytes
#define ALLOWED_ENDPOINT_ID    (APP_MESSAGE_ENDPOINT_ID)

static const uint8_t TEST_PAYLOAD[] = {0xaa, 0xbb, 0xcc, 0xdd};
static uint8_t TEST_EXPECTED_PP_MSG[sizeof(struct pbl_pp_header) + sizeof(TEST_PAYLOAD)];

void test_app_message_sender__initialize(void) {
  s_system_session_ptr = &s_system_session;
  s_app_session_ptr = &s_app_session;
  RESET_FAKE(app_install_mark_prioritized);
  RESET_FAKE(app_outbox_service_consume_message);
  RESET_FAKE(app_outbox_service_register);
  RESET_FAKE(pbl_pp_session_send_next);
  app_outbox_service_consume_message_fake.custom_fake = prv_consume_message;

  s_is_message_cancelled = false;

  s_process_md.allow_js = false;

  struct pbl_pp_header *header = (struct pbl_pp_header *)TEST_EXPECTED_PP_MSG;
  header->length = pbl_cpu_to_be16(sizeof(TEST_PAYLOAD));
  header->endpoint_id = pbl_cpu_to_be16(ALLOWED_ENDPOINT_ID);
  memcpy(TEST_EXPECTED_PP_MSG + sizeof(*header), TEST_PAYLOAD, sizeof(TEST_PAYLOAD));

  app_message_sender_init();
  cl_assert(app_outbox_service_register_fake.arg1_val);
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
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      s_system_session_ptr, DISALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorEndpointDisallowed, 1);
  app_free(outbox_data);
}

void test_app_message_sender__system_session_but_not_js_app(void) {
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      s_system_session_ptr, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  app_free(outbox_data);
}

void test_app_message_sender__app_session_but_js_app(void) {
  s_process_md.allow_js = true;
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      s_app_session_ptr, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  app_free(outbox_data);
}

void test_app_message_sender__no_sessions_connected(void) {
  s_system_session_ptr = nullptr;
  s_app_session_ptr = nullptr;
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  app_free(outbox_data);
}

void test_app_message_sender__auto_select_not_js_app(void) {
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  prv_process_send_queue(s_system_session_ptr);
  assert_not_consumed();

  prv_process_send_queue(s_app_session_ptr);
  assert_consumed(AppMessageSenderErrorSuccess, 1);

  app_free(outbox_data);
}

void test_app_message_sender__auto_select_js_app(void) {
  s_process_md.allow_js = true;
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  prv_process_send_queue(s_app_session_ptr);
  assert_not_consumed();

  prv_process_send_queue(s_system_session_ptr);
  assert_consumed(AppMessageSenderErrorSuccess, 1);

  app_free(outbox_data);
}

void test_app_message_sender__system_session_and_js_app(void) {
  s_process_md.allow_js = true;
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      s_system_session_ptr, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  assert_not_consumed();
  prv_process_send_queue(s_system_session_ptr);
  assert_consumed(AppMessageSenderErrorSuccess, 1);
  cl_assert_equal_i(app_install_mark_prioritized_fake.call_count, 1);
  app_free(outbox_data);
}

// Tests that exercise interface towards the Send Queue
////////////////////////////////////////////////////////////////////////////////////////////////////

void test_app_message_sender__freed_but_not_sent_entirely(void) {
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  size_t length = pbl_pp_session_send_queue_length(s_app_session_ptr);
  pbl_pp_session_send_queue_consume(s_app_session_ptr, length - 1);
  pbl_pp__send_queue_drop(s_app_session_ptr);
  assert_consumed(AppMessageSenderErrorDisconnected, 1);
  cl_assert_equal_i(app_install_mark_prioritized_fake.call_count, 0);
  app_free(outbox_data);
}

void test_app_message_sender__byte_by_byte_consume(void) {
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  size_t length = pbl_pp_session_send_queue_length(s_app_session_ptr);
  cl_assert_equal_i(length, sizeof(struct pbl_pp_header) + sizeof(TEST_PAYLOAD));

  for (int i = 0; i < length; ++i) {
    // Test the `length` implementation:
    cl_assert_equal_i(length - i, pbl_pp_session_send_queue_length(s_app_session_ptr));

    // Test the `read_pointer` implementation:
    const uint8_t *read_pointer = nullptr;
    size_t length_available = pbl_pp_session_send_queue_peek(s_app_session_ptr, &read_pointer);
    cl_assert(read_pointer);
    cl_assert_equal_i(TEST_EXPECTED_PP_MSG[i], *read_pointer);
    // Expect that the header and payload will be non-contiguous:
    if (i < sizeof(struct pbl_pp_header)) {
      cl_assert_equal_i(sizeof(struct pbl_pp_header) - i, length_available);
    } else {
      cl_assert_equal_i(length - i, length_available);
    }

    // Test the `copy` implementation:
    uint8_t byte_out = 0xff;
    cl_assert_equal_i(1, pbl_pp_session_send_queue_copy(s_app_session_ptr, 0 /* offset */,
                                                        1 /* length */, &byte_out));
    cl_assert_equal_i(TEST_EXPECTED_PP_MSG[i], byte_out);

    pbl_pp_session_send_queue_consume(s_app_session_ptr, 1 /* length */);
  }

  assert_consumed(AppMessageSenderErrorSuccess, 1);
  cl_assert_equal_i(app_install_mark_prioritized_fake.call_count, 1);
  app_free(outbox_data);
}

void test_app_message_sender__byte_by_byte_copy_with_offset(void) {
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
  size_t length = pbl_pp_session_send_queue_length(s_app_session_ptr);
  cl_assert_equal_i(length, sizeof(struct pbl_pp_header) + sizeof(TEST_PAYLOAD));

  uint8_t bytes_out[length];
  memset(bytes_out, 0xff, length);

  // Consume byte by byte:
  for (int c = 0; c < length; ++c) {
    // Shift offset byte by byte:
    for (int o = 0; o < (length - c); ++o) {
      size_t length_to_copy = (length - c - o);
      cl_assert_equal_i(length_to_copy, pbl_pp_session_send_queue_copy(s_app_session_ptr, o,
                                                                       length_to_copy, bytes_out));
      cl_assert_equal_i(0, memcmp(bytes_out, TEST_EXPECTED_PP_MSG + o + c, length_to_copy));
    }

    pbl_pp_session_send_queue_consume(s_app_session_ptr, 1 /* length */);
  }

  assert_consumed(AppMessageSenderErrorSuccess, 1);
  cl_assert_equal_i(app_install_mark_prioritized_fake.call_count, 1);
  app_free(outbox_data);
}

// Tests that deal with the edge case of app outbox messages getting cancelled,
// because the app that provides the buffer for the payload is quit while they are in the
// process of being sent out.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void prv_quit_app_after_pp_msg_byte(uint32_t num_bytes) {
  AppMessageAppOutboxData *outbox_data = prv_create_and_send_outbox_message(
      nullptr /* auto-select */, ALLOWED_ENDPOINT_ID, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));

  size_t length = pbl_pp_session_send_queue_length(s_app_session_ptr);
  uint8_t bytes_out[length];
  memset(bytes_out, 0xff, length);

  // Copy & consume one byte of the header -- note the header is 4 bytes total:
  size_t first_length = num_bytes;
  cl_assert_equal_i(first_length,
                    pbl_pp_session_send_queue_copy(s_app_session_ptr, 0, first_length, bytes_out));
  pbl_pp_session_send_queue_consume(s_app_session_ptr, first_length);

  // App quits with only one header byte consumed:
  app_outbox_service_cleanup_all_pending_messages();

  // Copy & consume the rest:
  size_t second_length = (length - first_length);
  cl_assert_equal_i(second_length,
                    pbl_pp_session_send_queue_copy(s_app_session_ptr, 0, second_length,
                                                   bytes_out + first_length));
  pbl_pp_session_send_queue_consume(s_app_session_ptr, second_length);

  // The message should be consumed now (to free the resources associated with it):
  assert_consumed(AppMessageSenderErrorSuccess, 1);

  // Expect at least the PebbleProtocol header or more to be intact:
  size_t intact_size = MAX(num_bytes, sizeof(struct pbl_pp_header));
  cl_assert_equal_m(bytes_out, TEST_EXPECTED_PP_MSG, intact_size);

  // Expect the remainder to be filled with zeroes:
  for (int i = 0; i < (length - intact_size); ++i) {
    cl_assert_equal_i(bytes_out[intact_size + i], 0x00);
  }

  app_free(outbox_data);
}

void test_app_message_sender__cancelled_message_in_flight_header_and_payload_not_finished(void) {
  // Expect header to get sent out normally, then a payload with all zeroes
  prv_quit_app_after_pp_msg_byte(sizeof(struct pbl_pp_header) - 1);
}

void test_app_message_sender__cancelled_message_in_flight_payload_not_finished(void) {
  // Expect remainder payload to be all zeroes
  prv_quit_app_after_pp_msg_byte(sizeof(struct pbl_pp_header) + 1);
}
