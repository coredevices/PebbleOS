/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "test_jerry_port_common.h"
#include "test_rocky_common.h"

#include "applib/rockyjs/api/rocky_api_global.h"
#include "applib/rockyjs/api/rocky_api_app_message.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include "applib/app_message/app_message.h"
#include "util/dict.h"
#include "util/size.h"

#include <string.h>

// Fakes
#include "fake_app_timer.h"
#include "fake_event_service.h"
#include "fake_pbl_malloc.h"
#include "fake_time.h"

// Stubs
#include "stubs_app_state.h"
#include "stubs_comm_session.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_serial.h"
#include "stubs_sys_exit.h"

extern PostMessageState rocky_api_app_message_get_state(void);
extern AppTimer *rocky_api_app_message_get_app_msg_retry_timer(void);
extern AppTimer *rocky_api_app_message_get_session_closed_object_queue_timer(void);

T_STATIC jerry_value_t prv_json_stringify(jerry_value_t object);
T_STATIC jerry_value_t prv_json_parse(const char *);

T_STATIC void prv_handle_connection(void);
T_STATIC void prv_handle_disconnection(void);

// App message mocks

static AppMessageInboxReceived s_received_callback;
AppMessageInboxReceived app_message_register_inbox_received(
                                                        AppMessageInboxReceived received_callback) {
  AppMessageInboxReceived prev_cb = s_received_callback;
  s_received_callback = received_callback;
  return prev_cb;
}

static AppMessageInboxDropped s_dropped_callback;
AppMessageInboxDropped app_message_register_inbox_dropped(AppMessageInboxDropped dropped_callback) {
  AppMessageInboxDropped prev_cb = s_dropped_callback;
  s_dropped_callback = dropped_callback;
  return prev_cb;
}

static AppMessageOutboxSent s_sent_callback;
AppMessageOutboxSent app_message_register_outbox_sent(AppMessageOutboxSent sent_callback) {
  AppMessageOutboxSent prev_cb = s_sent_callback;
  s_sent_callback = sent_callback;
  return prev_cb;
}

static AppMessageOutboxFailed s_failed_callback;
AppMessageOutboxFailed app_message_register_outbox_failed(AppMessageOutboxFailed failed_callback) {
  AppMessageOutboxFailed prev_cb = s_failed_callback;
  s_failed_callback = failed_callback;
  return prev_cb;
}

void app_message_deregister_callbacks(void) {
  s_received_callback = NULL;
  s_dropped_callback = NULL;
  s_sent_callback = NULL;
  s_failed_callback = NULL;
}

static uint32_t s_inbox_size;
static uint32_t s_outbox_size;
AppMessageResult app_message_open(const uint32_t size_inbound, const uint32_t size_outbound) {
  s_inbox_size = size_inbound;
  s_outbox_size = size_outbound;
  return APP_MSG_OK;
}

static bool s_is_outbox_message_pending;
static DictionaryIterator s_outbox_iterator;
static uint8_t *s_outbox_buffer;
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator) {
  cl_assert_equal_b(s_is_outbox_message_pending, false);
  if (!s_outbox_buffer) {
    s_outbox_buffer = malloc(s_outbox_size);
  }
  dict_write_begin(&s_outbox_iterator, s_outbox_buffer, s_outbox_size);
  *iterator = &s_outbox_iterator;

  return APP_MSG_OK;
}

static int s_app_message_outbox_send_call_count;
AppMessageResult app_message_outbox_send(void) {
  ++s_app_message_outbox_send_call_count;
  s_is_outbox_message_pending = true;
  return APP_MSG_OK;
}

static bool s_comm_session_connected;
CommSession *sys_app_pp_get_comm_session(void) {
  return (CommSession *)s_comm_session_connected;
}

static void prv_rcv_app_message_ack(AppMessageResult result) {
  void *context = NULL;
  cl_assert_equal_b(s_is_outbox_message_pending, true);
  s_is_outbox_message_pending = false;
  if (result == APP_MSG_OK) {
    s_sent_callback(&s_outbox_iterator, context);
  } else {
    s_failed_callback(&s_outbox_iterator, result, context);
  }
}

static void prv_app_message_setup(void) {
  s_inbox_size = 0;
  s_outbox_size = 0;
  s_outbox_buffer = NULL;
  s_app_message_outbox_send_call_count = 0;
  s_is_outbox_message_pending = false;
  app_message_deregister_callbacks();
}

static void prv_app_message_teardown(void) {
  if (s_outbox_buffer) {
    free(s_outbox_buffer);
  }
}

// Statics and Utilities

static void prv_init_and_goto_session_open(void);

static void prv_simulate_transport_connection_event(bool is_connected) {
  // FIXME: use events here instead of poking at the internals!
  if (is_connected) {
    prv_handle_connection();
  } else {
    prv_handle_disconnection();
  }
}

static const RockyGlobalAPI *s_app_message_api[] = {
  &APP_MESSAGE_APIS,
  NULL,
};

static void prv_init_api(bool start_connected) {
  s_comm_session_connected = start_connected;
  rocky_global_init(s_app_message_api);
}

// Setup

void test_rocky_api_app_message__initialize(void) {
  fake_app_timer_init();
  fake_pbl_malloc_clear_tracking();
  prv_app_message_setup();

  s_process_manager_callback = NULL;
  s_process_manager_callback_data = NULL;

  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
}

void test_rocky_api_app_message__cleanup(void) {
  rocky_global_deinit();
  jerry_cleanup();
  rocky_runtime_context_deinit();
  prv_app_message_teardown();
  fake_pbl_malloc_check_net_allocs();
  fake_app_timer_deinit();
}

static const PostMessageResetCompletePayload VALID_RESET_COMPLETE = {
  .min_supported_version = POSTMESSAGE_PROTOCOL_MIN_VERSION,
  .max_supported_version = POSTMESSAGE_PROTOCOL_MAX_VERSION,
  .max_tx_chunk_size = POSTMESSAGE_PROTOCOL_MAX_TX_CHUNK_SIZE,
  .max_rx_chunk_size = POSTMESSAGE_PROTOCOL_MAX_RX_CHUNK_SIZE,
};

static const size_t TINY_CHUNK_SIZE = 4;

static const PostMessageResetCompletePayload TINY_RESET_COMPLETE = {
  .min_supported_version = POSTMESSAGE_PROTOCOL_MIN_VERSION,
  .max_supported_version = POSTMESSAGE_PROTOCOL_MAX_VERSION,
  .max_tx_chunk_size = TINY_CHUNK_SIZE,
  .max_rx_chunk_size = TINY_CHUNK_SIZE,
};

#define RCV_APP_MESSAGE(...) \
  do { \
    Tuplet tuplets[] = { __VA_ARGS__ }; \
    uint32_t buffer_size = dict_calc_buffer_size_from_tuplets(tuplets, ARRAY_LENGTH(tuplets)); \
    uint8_t buffer[buffer_size]; \
    DictionaryIterator it; \
    const DictionaryResult result = \
        dict_serialize_tuplets_to_buffer_with_iter(&it, tuplets, ARRAY_LENGTH(tuplets), \
                                                   buffer, &buffer_size); \
    cl_assert_equal_i(DICT_OK, result); \
    if (s_received_callback) { \
      s_received_callback(&it, NULL); \
    } \
  } while(0);


#define RCV_RESET_REQUEST() \
  RCV_APP_MESSAGE(TupletBytes(PostMessageKeyResetRequest, NULL, 0));

#define RCV_RESET_COMPLETE() \
  RCV_APP_MESSAGE(TupletBytes(PostMessageKeyResetComplete, \
                  (const uint8_t *)&VALID_RESET_COMPLETE, sizeof(VALID_RESET_COMPLETE)));

#define RCV_DUMMY_CHUNK() \
  do { \
    PostMessageChunkPayload chunk = {}; \
    RCV_APP_MESSAGE(TupletBytes(PostMessageKeyChunk, (const uint8_t *) &chunk, sizeof(chunk))); \
  } while(0);

//! Asserts whether the outbox has a pending message containing the tuples passed to this macro.
//! The value and type of the tuples is also asserted.
//! @note Only asserts if expected tuples are MISSING. It will not trip if there are other
//! (non-expected) tuples in the set.
#define EXPECT_OUTBOX_MESSAGE_PENDING(...) \
  do { \
    cl_assert_equal_b(true, s_is_outbox_message_pending); \
    /* The cursor must be updated! */ \
    cl_assert(s_outbox_iterator.cursor != s_outbox_iterator.dictionary->head); \
    Tuplet tuplets[] = { __VA_ARGS__ }; \
    uint32_t buffer_size = dict_calc_buffer_size_from_tuplets(tuplets, ARRAY_LENGTH(tuplets)); \
    uint8_t buffer[buffer_size]; \
    DictionaryIterator expected_it; \
    const DictionaryResult result = \
        dict_serialize_tuplets_to_buffer_with_iter(&expected_it, tuplets, ARRAY_LENGTH(tuplets), \
                                                   buffer, &buffer_size); \
    cl_assert_equal_i(DICT_OK, result); \
    for (Tuple *expected_t = dict_read_first(&expected_it); expected_t != NULL; \
         expected_t = dict_read_next(&expected_it)) { \
      Tuple *found_t = dict_find(&s_outbox_iterator, expected_t->key); \
      cl_assert(found_t); \
      cl_assert_equal_i(found_t->type, expected_t->type); \
      cl_assert_equal_i(found_t->length, expected_t->length); \
      if (expected_t->length) { \
        cl_assert_equal_i(0, memcmp(found_t->value[0].data, expected_t->value[0].data, \
                                    expected_t->length)); \
      } \
    } \
  } while (0);

#define EXPECT_OUTBOX_NO_MESSAGE_PENDING() \
  cl_assert_equal_b(false, s_is_outbox_message_pending);

#define EXPECT_OUTBOX_RESET_REQUEST() \
  EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyResetRequest, NULL, 0));

#define EXPECT_OUTBOX_RESET_COMPLETE_PENDING() \
  EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyResetComplete, \
                                            (const uint8_t *) &VALID_RESET_COMPLETE, \
                                            sizeof(VALID_RESET_COMPLETE)));

////////////////////////////////////////////////////////////////////////////////
// Negotiation Steps
////////////////////////////////////////////////////////////////////////////////

void test_rocky_api_app_message__disconnected__ignore_any_app_message(void) {
  prv_init_api(false /* start_connected */);

  for (PostMessageKey key = PostMessageKeyResetRequest; key < PostMessageKey_Count; ++key) {
    uint8_t dummy_data[] = {0, 1, 2};
    RCV_APP_MESSAGE(TupletBytes(key, dummy_data, sizeof(dummy_data)));
  }

  cl_assert_equal_i(0, s_app_message_outbox_send_call_count);
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateDisconnected);
}

void test_rocky_api_app_message__awaiting_reset_request__receive_reset_request(void) {
  prv_init_api(true /* start_connected */);

  RCV_RESET_REQUEST();

  // Expect responding with a ResetComplete:
  EXPECT_OUTBOX_RESET_COMPLETE_PENDING();

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteRemoteInitiated);
}

void test_rocky_api_app_message__awaiting_reset_request__receive_chunk(void) {
  prv_init_api(false /* start_connected */);
  prv_simulate_transport_connection_event(true /* is_connected */);

  RCV_DUMMY_CHUNK();
  // https://pebbletechnology.atlassian.net/browse/PBL-42466
  // TODO: assert that app message was NACK'd

  // Expect responding with a ResetRequest:
  EXPECT_OUTBOX_RESET_REQUEST();
  EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyResetRequest, NULL, 0));
  // TODO: check fields

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteLocalInitiated);
}

void test_rocky_api_app_message__awaiting_reset_request__disconnect(void) {
  prv_init_api(false /* start_connected */);
  prv_simulate_transport_connection_event(true /* is_connected */);
  prv_simulate_transport_connection_event(false /* is_connected */);
  EXPECT_OUTBOX_NO_MESSAGE_PENDING();
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateDisconnected);
}

static void prv_init_and_goto_awaiting_reset_complete_remote_initiated(void) {
  prv_init_api(true /* start_connected */);

  RCV_RESET_REQUEST();

  EXPECT_OUTBOX_RESET_COMPLETE_PENDING();
  prv_rcv_app_message_ack(APP_MSG_OK);

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteRemoteInitiated);
}

static void prv_init_and_goto_awaiting_reset_complete_local_initiated(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();
  RCV_DUMMY_CHUNK();
  EXPECT_OUTBOX_RESET_REQUEST();
  prv_rcv_app_message_ack(APP_MSG_OK);
  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteLocalInitiated);
}

void test_rocky_api_app_message__awaiting_reset_complete_rem_init__receive_complete_valid_version(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();

  RCV_RESET_COMPLETE();

  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateSessionOpen);
  EXPECT_OUTBOX_NO_MESSAGE_PENDING();
}

void test_rocky_api_app_message__awaiting_reset_complete_rem_init__receive_complete_unsupported_ver(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();

  const PostMessageResetCompletePayload unsupported = {
    .min_supported_version = POSTMESSAGE_PROTOCOL_MAX_VERSION + 1,
    .max_supported_version = POSTMESSAGE_PROTOCOL_MAX_VERSION + 1,
    .max_tx_chunk_size = POSTMESSAGE_PROTOCOL_MAX_TX_CHUNK_SIZE,
    .max_rx_chunk_size = POSTMESSAGE_PROTOCOL_MAX_RX_CHUNK_SIZE,
  };
  RCV_APP_MESSAGE(TupletBytes(PostMessageKeyResetComplete,
                              (const uint8_t *)&unsupported, sizeof(unsupported)));

  // Expect No UnsupportedError!

  // Immediately go back to AwaitingResetRequest:
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateAwaitingResetRequest);
  EXPECT_OUTBOX_NO_MESSAGE_PENDING();
}

void test_rocky_api_app_message__awaiting_reset_complete_rem_init__malformed_reset_complete(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();

  // Receive malformed ResetComplete:
  uint8_t malformed_payload[sizeof(PostMessageResetCompletePayload) - 1] = {};
  RCV_APP_MESSAGE(TupletBytes(PostMessageKeyResetComplete,
                              malformed_payload, sizeof(malformed_payload)));

  // Expect Error:
  const PostMessageUnsupportedErrorPayload expected_error = {
    .error_code = PostMessageErrorMalformedResetComplete,
  };
  EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyUnsupportedError,
                                            (const uint8_t *) &expected_error,
                                            sizeof(expected_error)));

  // Immediately go back to AwaitingResetRequest:
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateAwaitingResetRequest);
  prv_rcv_app_message_ack(APP_MSG_OK);
  EXPECT_OUTBOX_NO_MESSAGE_PENDING();
}

void test_rocky_api_app_message__awaiting_reset_complete_rem_init__receive_request(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();

  RCV_RESET_REQUEST();

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteRemoteInitiated);

  EXPECT_OUTBOX_RESET_COMPLETE_PENDING();
}

void test_rocky_api_app_message__awaiting_reset_complete_rem_init__receive_chunk(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();

  RCV_DUMMY_CHUNK();

  EXPECT_OUTBOX_RESET_REQUEST();

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteLocalInitiated);

  // Receive yet another chunk in "Awaiting Reset Complete Local Initiated":
  RCV_DUMMY_CHUNK();
  // https://pebbletechnology.atlassian.net/browse/PBL-42466
  // TODO: assert that chunk is NACKd

  // Receive ACK for the ResetRequest:
  prv_rcv_app_message_ack(APP_MSG_OK);

  // Chunk is ignored, no new reset request is sent out.
  EXPECT_OUTBOX_NO_MESSAGE_PENDING();

  // TODO: timeout + retry ResetRequest if no ResetComplete follows within N secs.
}

void test_rocky_api_app_message__awaiting_reset_complete_loc_init__(void) {
  prv_init_and_goto_awaiting_reset_complete_local_initiated();
}

void test_rocky_api_app_message__awaiting_reset_complete_loc_init__rcv_reset_request(void) {
  prv_init_and_goto_awaiting_reset_complete_local_initiated();

  RCV_RESET_REQUEST();

  EXPECT_OUTBOX_RESET_COMPLETE_PENDING();
  prv_rcv_app_message_ack(APP_MSG_OK);

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteRemoteInitiated);

  RCV_RESET_COMPLETE();

  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateSessionOpen);
}

void test_rocky_api_app_message__awaiting_reset_complete_loc_init__rcv_chunk(void) {
  prv_init_and_goto_awaiting_reset_complete_local_initiated();

  RCV_DUMMY_CHUNK();

  // https://pebbletechnology.atlassian.net/browse/PBL-42466
  // TODO: assert that chunk is NACK'd

  // Chunk is ignored, state isn't changed:
  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteLocalInitiated);
  EXPECT_OUTBOX_NO_MESSAGE_PENDING();
}

static void prv_init_and_goto_session_open(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();
  RCV_RESET_COMPLETE();
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateSessionOpen);
}

void test_rocky_api_app_message__session_open__rcv_reset_request(void) {
  prv_init_and_goto_session_open();

  EXECUTE_SCRIPT("var isCalled = false;"
                 "_rocky.on('postmessagedisconnected', function() { isCalled = true; });");

  ASSERT_JS_GLOBAL_EQUALS_B("isCalled", false);

  RCV_RESET_REQUEST();

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteRemoteInitiated);
  EXPECT_OUTBOX_RESET_COMPLETE_PENDING();

  ASSERT_JS_GLOBAL_EQUALS_B("isCalled", true);

  // TODO: assert:
  // - flushed recv chunk reassembly buffer
}

void test_rocky_api_app_message__session_open__rcv_reset_complete(void) {
  prv_init_and_goto_session_open();

  EXECUTE_SCRIPT("var isCalled = false;"
                 "_rocky.on('postmessagedisconnected', function() { isCalled = true; });");

  ASSERT_JS_GLOBAL_EQUALS_B("isCalled", false);

  RCV_RESET_COMPLETE();

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteLocalInitiated);
  EXPECT_OUTBOX_RESET_REQUEST();

  ASSERT_JS_GLOBAL_EQUALS_B("isCalled", true);

  // TODO: assert:
  // - flushed recv chunk reassembly buffer
}

////////////////////////////////////////////////////////////////////////////////
// postmessageconnected / postmessagedisconnected
////////////////////////////////////////////////////////////////////////////////

static void prv_postmessageconnected_postmessagedisconnected_init(bool start_connected) {
  prv_init_api(start_connected);

  EXECUTE_SCRIPT("var c = 0; var d = 0;\n"
                 "_rocky.on('postmessageconnected', function() { c++; });\n"
                 "_rocky.on('postmessagedisconnected', function() { d++; });\n");

  // Make sure this race is handled (see comment in prv_handle_connection()):
  prv_simulate_transport_connection_event(start_connected /* is_connected */);
}

static void prv_postmessageconnected_postmessagedisconnected_negotiate_to_open_session(void) {
  // Negotiate:
  RCV_RESET_REQUEST();

  EXPECT_OUTBOX_RESET_COMPLETE_PENDING();
  prv_rcv_app_message_ack(APP_MSG_OK);

  cl_assert_equal_i(rocky_api_app_message_get_state(),
                    PostMessageStateAwaitingResetCompleteRemoteInitiated);

  RCV_RESET_COMPLETE();
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateSessionOpen);
}

void test_rocky_api_app_message__postmessageconnected_and_postmessagedisconnected_remote_rr(void) {
  prv_postmessageconnected_postmessagedisconnected_init(false /* start_connected */);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);
  prv_simulate_transport_connection_event(true /* is_connected */);
  ASSERT_JS_GLOBAL_EQUALS_I("c", 0);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);
  prv_postmessageconnected_postmessagedisconnected_negotiate_to_open_session();
  ASSERT_JS_GLOBAL_EQUALS_I("c", 1);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);

  // Get a ResetRequest:
  RCV_RESET_REQUEST();
  ASSERT_JS_GLOBAL_EQUALS_I("d", 2);
}

void test_rocky_api_app_message__postmessageconnected_and_postmessagedisconnected_local_rr(void) {
  prv_postmessageconnected_postmessagedisconnected_init(false /* start_connected */);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);
  prv_simulate_transport_connection_event(true /* is_connected */);
  ASSERT_JS_GLOBAL_EQUALS_I("c", 0);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);
  prv_postmessageconnected_postmessagedisconnected_negotiate_to_open_session();
  ASSERT_JS_GLOBAL_EQUALS_I("c", 1);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);

  // Get a ResetComplete (unexpected message), should trigger initiating (local) ResetRequest:
  RCV_RESET_COMPLETE();
  ASSERT_JS_GLOBAL_EQUALS_I("d", 2);
}

void test_rocky_api_app_message__postmessageconnected_and_postmessagedisconnected_start_conn(void) {
  prv_postmessageconnected_postmessagedisconnected_init(true /* start_connected */);
  ASSERT_JS_GLOBAL_EQUALS_I("c", 0);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);

  prv_postmessageconnected_postmessagedisconnected_negotiate_to_open_session();

  ASSERT_JS_GLOBAL_EQUALS_I("c", 1);
  ASSERT_JS_GLOBAL_EQUALS_I("d", 1);
}

// TODO: test various min/max version combos
// TODO: test RX/TX buffer size combos

////////////////////////////////////////////////////////////////////////////////
// Generic Tests
////////////////////////////////////////////////////////////////////////////////

void test_rocky_api_app_message__json_stringify(void) {
  JS_VAR obj = jerry_create_object();
  JS_VAR json_str = prv_json_stringify(obj);
  char *json_c_str = rocky_string_alloc_and_copy(json_str);
  cl_assert_equal_s(json_c_str, "{}");
  task_free(json_c_str);
}

void test_rocky_api_app_message__json_parse(void) {
  JS_VAR number = prv_json_parse("1");
  cl_assert(jerry_value_is_number(number));
  cl_assert_equal_d(jerry_get_number_value(number), 1.0);

  JS_VAR object = prv_json_parse("{ \"x\" : 42 }");
  cl_assert(jerry_value_is_object(object));
  JS_VAR x = jerry_get_object_field(object, "x");
  cl_assert(jerry_value_is_number(x));
  cl_assert_equal_d(jerry_get_number_value(x), 42.0);
}

void test_rocky_api_app_message__json_parse_stress(void) {
  const int num_attempts = 0x3ff + 10; // Want this to be higher than the max refcount,
                                       // which will also be high enough for a memory stress test
  for (int i = 0; i < num_attempts; ++i) {
    JS_UNUSED_VAL = prv_json_parse(
        "var msg = { "
        "\"key\" : "
        "\"Bacon ipsum dolor amet kevin filet mignon id ut, aute sausage tri-tip "
        "frankfurter pork loin. Boudin ullamco landjaeger, kevin tongue minim tri-tip "
        "ground round dolore. Ham hock tongue swine, cillum jowl pancetta fugiat "
        "deserunt sirloin fatback tenderloin culpa andouille. Incididunt qui bacon "
        "nostrud ham hock adipisicing et ham. Ullamco esse eu capicola, ea culpa irure "
        "meatball proident laboris ut reprehenderit ex incididunt.\" };\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
// .postMessage() Tests
////////////////////////////////////////////////////////////////////////////////

#define SIMPLE_TEST_OBJECT "{ \"x\" : 1 }"

static void prv_assert_simple_test_object_pending(void) {
  const char * const expected_json = "{\"x\":1}";
  const size_t  expected_json_size = strlen(expected_json) + 1;
  const size_t expected_size = sizeof(PostMessageChunkPayload) + strlen(expected_json) + 1;
  uint8_t *buffer = task_malloc(expected_size);

  PostMessageChunkPayload *chunk = (PostMessageChunkPayload *)buffer;
  *chunk = (PostMessageChunkPayload) {
    .total_size_bytes = expected_json_size,
    .is_first = true,
  };
  memcpy(chunk->chunk_data, expected_json, expected_json_size);

  EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyChunk,
                                            (const uint8_t *) chunk, expected_size));

  // Compare with hard-coded byte array, to catch accidental changes to the ABI:
  const uint8_t raw_bytes_v1[] = {
    0x08, 0x00, 0x00, 0x80, 0x7b, 0x22, 0x78, 0x22, 0x3a, 0x31, 0x7d, 0x00,
  };
  cl_assert_equal_i(sizeof(raw_bytes_v1), expected_size);
  cl_assert_equal_m(raw_bytes_v1, buffer, expected_size);

  task_free(buffer);
}

void test_rocky_api_app_message__postmessage_just_before_connected(void) {
  prv_init_api(false /* start_connected */);

  EXECUTE_SCRIPT("var x = " SIMPLE_TEST_OBJECT ";"
                 "var hasError = false;"
                 "_rocky.on('postmessageerror', function() { hasError = true; });"
                 "_rocky.postMessage(x);");

  // First send attempt fails because not in SessionOpen
  ASSERT_JS_GLOBAL_EQUALS_B("hasError", false);

  prv_simulate_transport_connection_event(true /* is_connected */);
  prv_postmessageconnected_postmessagedisconnected_negotiate_to_open_session();

  prv_assert_simple_test_object_pending();

  prv_rcv_app_message_ack(APP_MSG_OK);

  EXPECT_OUTBOX_NO_MESSAGE_PENDING();

  ASSERT_JS_GLOBAL_EQUALS_B("hasError", false);
}

void test_rocky_api_app_message__post_message_single_chunk(void) {
  prv_init_and_goto_session_open();

  EXECUTE_SCRIPT("var x = " SIMPLE_TEST_OBJECT "; _rocky.postMessage(x);");
  prv_assert_simple_test_object_pending();

  prv_rcv_app_message_ack(APP_MSG_OK);

  EXPECT_OUTBOX_NO_MESSAGE_PENDING();
}

static void prv_init_and_goto_session_open_with_tiny_buffers(void) {
  prv_init_and_goto_awaiting_reset_complete_remote_initiated();
  RCV_APP_MESSAGE(TupletBytes(PostMessageKeyResetComplete, \
                              (const uint8_t *)&TINY_RESET_COMPLETE, sizeof(TINY_RESET_COMPLETE)));
  cl_assert_equal_i(rocky_api_app_message_get_state(), PostMessageStateSessionOpen);
}

void test_rocky_api_app_message__post_message_multi_chunk(void) {
  prv_init_and_goto_session_open_with_tiny_buffers();

  EXECUTE_SCRIPT("var x = { \"x\" : 123 }; _rocky.postMessage(x);");

  const char * const expected_json = "{\"x\":123}";
  const size_t expected_json_size = strlen(expected_json) + 1;
  size_t json_bytes_remaining = expected_json_size;

  uint8_t *buffer = task_malloc(sizeof(PostMessageChunkPayload) + TINY_CHUNK_SIZE);

  // Chunk 1:
  {
    const size_t json_bytes_size = MIN(TINY_CHUNK_SIZE, json_bytes_remaining);
    const size_t expected_size = sizeof(PostMessageChunkPayload) + json_bytes_size;

    PostMessageChunkPayload *chunk = (PostMessageChunkPayload *)buffer;
    *chunk = (PostMessageChunkPayload) {
      .total_size_bytes = expected_json_size,
      .is_first = true,
    };
    memcpy(chunk->chunk_data, expected_json, TINY_CHUNK_SIZE);

    EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyChunk,
                                              (const uint8_t *) chunk, expected_size));

    // Compare with hard-coded byte array, to catch accidental changes to the ABI:
    const uint8_t raw_bytes_v1[] = {
      0x0a, 0x00, 0x00, 0x80, '{', '"', 'x', '"',
    };
    cl_assert_equal_i(sizeof(raw_bytes_v1), expected_size);
    cl_assert_equal_m(raw_bytes_v1, buffer, expected_size);

    prv_rcv_app_message_ack(APP_MSG_OK);
    json_bytes_remaining -= json_bytes_size;
  }

  // Chunk 2:
  {
    const size_t json_bytes_size = MIN(TINY_CHUNK_SIZE, json_bytes_remaining);
    const size_t expected_size = sizeof(PostMessageChunkPayload) + json_bytes_size;
    const int payload_offset = expected_json_size - json_bytes_remaining;

    PostMessageChunkPayload *chunk = (PostMessageChunkPayload *)buffer;
    *chunk = (PostMessageChunkPayload) {
      .offset_bytes = payload_offset,
      .continuation_is_first = false,
    };
    memcpy(chunk->chunk_data, expected_json + payload_offset, TINY_CHUNK_SIZE);

    EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyChunk,
                                              (const uint8_t *) chunk, expected_size));

    // Compare with hard-coded byte array, to catch accidental changes to the ABI:
    const uint8_t raw_bytes_v1[] = {
      0x04, 0x00, 0x00, 0x00, ':', '1', '2', '3',
    };
    cl_assert_equal_i(sizeof(raw_bytes_v1), expected_size);
    cl_assert_equal_m(raw_bytes_v1, buffer, expected_size);

    prv_rcv_app_message_ack(APP_MSG_OK);
    json_bytes_remaining -= json_bytes_size;
  }

  // Chunk 3:
  {
    const size_t json_bytes_size = MIN(TINY_CHUNK_SIZE, json_bytes_remaining);
    const size_t expected_size = sizeof(PostMessageChunkPayload) + json_bytes_size;
    const int payload_offset = expected_json_size - json_bytes_remaining;

    PostMessageChunkPayload *chunk = (PostMessageChunkPayload *)buffer;
    *chunk = (PostMessageChunkPayload) {
      .offset_bytes = payload_offset,
      .continuation_is_first = false,
    };
    memcpy(chunk->chunk_data, expected_json + payload_offset, TINY_CHUNK_SIZE);

    EXPECT_OUTBOX_MESSAGE_PENDING(TupletBytes(PostMessageKeyChunk,
                                              (const uint8_t *) chunk, expected_size));

    // Compare with hard-coded byte array, to catch accidental changes to the ABI:
    const uint8_t raw_bytes_v1[] = {
      0x08, 0x00, 0x00, 0x00, '}', '\0',
    };
    cl_assert_equal_i(sizeof(raw_bytes_v1), expected_size);
    cl_assert_equal_m(raw_bytes_v1, buffer, expected_size);

    prv_rcv_app_message_ack(APP_MSG_OK);
    json_bytes_remaining -= json_bytes_size;
  }

  EXPECT_OUTBOX_NO_MESSAGE_PENDING();

  task_free(buffer);
}

void test_rocky_api_app_message__postmessage_not_jsonable(void) {
  prv_init_and_goto_session_open();

  const char *not_jsonable_error =
  "TypeError: Argument at index 0 is not a JSON.stringify()-able object";

  EXECUTE_SCRIPT_EXPECT_ERROR("_rocky.postMessage(undefined);", not_jsonable_error);
  EXECUTE_SCRIPT_EXPECT_ERROR("_rocky.postMessage(function() {});", not_jsonable_error);
  EXECUTE_SCRIPT_EXPECT_ERROR("_rocky.postMessage({toJSON: function() {throw 'toJSONError';}});",
                              "toJSONError");
}

void test_rocky_api_app_message__postmessage_no_args(void) {
  prv_init_api(false /* start_connected */);
  EXECUTE_SCRIPT_EXPECT_ERROR("_rocky.postMessage();", "TypeError: Not enough arguments");
}

void test_rocky_api_app_message__postmessage_oom(void) {
  prv_init_api(false /* start_connected */);

  fake_malloc_set_largest_free_block(0);

  EXECUTE_SCRIPT_EXPECT_ERROR("_rocky.postMessage('x');",
                              "RangeError: Out of memory: can't postMessage() -- object too large");
}

////////////////////////////////////////////////////////////////////////////////
// Receive Tests
////////////////////////////////////////////////////////////////////////////////
void test_rocky_api_app_message__receive_message_multi_chunk(void) {
  prv_init_and_goto_session_open_with_tiny_buffers();

  EXECUTE_SCRIPT("var event = null;\n"
                 "var json_str = null;\n"
                 "_rocky.on('message', function(e) {\n"
                 "  json_str = JSON.stringify(e.data);\n" // stringify again to make assert simple
                 "  event = e;\n"
                 "});");
  JS_VAR event_null = prv_js_global_get_value("event");
  cl_assert_equal_b(true, jerry_value_is_null(event_null));

  // Get 3x the same message in a row:
  for (int j = 0; j < 3; ++j) {

    // Chunks for: {"x":123}
    const struct {
      uint8_t byte_array[8];
      size_t length;
    } chunk_msg_defs[] = {
      {
        .byte_array = {0x0a, 0x00, 0x00, 0x80, '{', '"', 'x', '"'},
        .length = 8,
      },
      {
        .byte_array = {0x04, 0x00, 0x00, 0x00, ':', '1', '2', '3'},
        .length = 8,
      },
      {
        .byte_array = {0x08, 0x00, 0x00, 0x00, '}', '\0'},
        .length = 6,
      }
    };

    for (int i = 0; i < ARRAY_LENGTH(chunk_msg_defs); ++i) {
      RCV_APP_MESSAGE(TupletBytes(PostMessageKeyChunk,
                                  (const uint8_t *) chunk_msg_defs[i].byte_array,
                                  chunk_msg_defs[i].length));
    }

    JS_VAR event_valid = prv_js_global_get_value("event");
    cl_assert_equal_b(true, jerry_value_is_object(event_valid));

    // Make sure that there is a "data" property
    JS_VAR data_prop = jerry_get_object_field(event_valid, "data");
    cl_assert_equal_b(true, jerry_value_is_object(data_prop));

    // Make sure the re-serialized object matches:
    ASSERT_JS_GLOBAL_EQUALS_S("json_str", "{\"x\":123}");

    EXECUTE_SCRIPT("json_str = null;\n"
                   "event = null");
  }
}

////////////////////////////////////////////////////////////////////////////////
// "postmessageerror" event
////////////////////////////////////////////////////////////////////////////////

void test_rocky_api_app_message__postmessageerror(void) {
  prv_init_and_goto_session_open();

  EXECUTE_SCRIPT("var didError = false;"
                 "var x = { \"x\" : 1 };"
                 "var dataJSON = undefined;"
                 "_rocky.on('postmessageerror', "
                 "          function(e) { didError = true; dataJSON = JSON.stringify(e.data); });"
                 "_rocky.postMessage(x);"
                 "x.x = 2;");

  ASSERT_JS_GLOBAL_EQUALS_B("didError", false);

  for (int i = 0; i < 3; ++i) {
    cl_assert_equal_b(s_is_outbox_message_pending, true);

    // NACK
    prv_rcv_app_message_ack(APP_MSG_BUSY);

    AppTimer *t = rocky_api_app_message_get_app_msg_retry_timer();
    cl_assert(t != EVENTED_TIMER_INVALID_ID);
    cl_assert_equal_b(fake_app_timer_is_scheduled(t), true);

    // Enqueuing more objects shouldn't affect the pace at which things are retried:
    EXECUTE_SCRIPT("_rocky.postMessage('')");

    EXPECT_OUTBOX_NO_MESSAGE_PENDING();

    cl_assert_equal_b(app_timer_trigger(t), true);
  }

  ASSERT_JS_GLOBAL_EQUALS_B("didError", true);
  ASSERT_JS_GLOBAL_EQUALS_S("dataJSON", "{\"x\":1}");
}

void test_rocky_api_app_message__postmessageerror_while_disconnected(void) {
  prv_init_api(false /* start_connected */);

  EXECUTE_SCRIPT("var didError = false;"
                 "var x = " SIMPLE_TEST_OBJECT ";"
                 "_rocky.on('postmessageerror', "
                 "          function(e) { didError = true; dataJSON = JSON.stringify(e.data); });"
                 /* 3x postMessage(): */
                 "_rocky.postMessage(x);"
                 "_rocky.postMessage(x);"
                 "_rocky.postMessage(x);");

  // Let the first 2 timeout:
  for (int i = 0; i < 2; ++i) {
    ASSERT_JS_GLOBAL_EQUALS_B("didError", false);

    AppTimer *t = rocky_api_app_message_get_session_closed_object_queue_timer();
    cl_assert(t != EVENTED_TIMER_INVALID_ID);
    cl_assert_equal_b(fake_app_timer_is_scheduled(t), true);

    EXPECT_OUTBOX_NO_MESSAGE_PENDING();

    cl_assert_equal_b(app_timer_trigger(t), true);

    ASSERT_JS_GLOBAL_EQUALS_B("didError", true);

    EXECUTE_SCRIPT("didError = false;");
  }

  // Timer for the 3rd should be set:
  AppTimer *t = rocky_api_app_message_get_session_closed_object_queue_timer();
  cl_assert(t != EVENTED_TIMER_INVALID_ID);
  cl_assert_equal_b(fake_app_timer_is_scheduled(t), true);

  // Connect:
  prv_simulate_transport_connection_event(true /* is_connected */);
  prv_postmessageconnected_postmessagedisconnected_negotiate_to_open_session();

  // Timer for the 3rd should be cancelled now:
  cl_assert(EVENTED_TIMER_INVALID_ID == rocky_api_app_message_get_session_closed_object_queue_timer());

  prv_assert_simple_test_object_pending();
}
