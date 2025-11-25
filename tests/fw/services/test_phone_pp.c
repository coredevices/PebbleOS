/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/phone_pp.h"
#include "services/normal/phone_call_util.h"

#include "services/common/comm_session/session.h"
#include "kernel/events.h"

#include "fake_events.h"
#include "fake_session.h"
#include "fake_system_task.h"

#include "stubs_bt_lock.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_hexdump.h"
#include "stubs_serial.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_i18n.h"

extern void phone_protocol_msg_callback(CommSession *session, const uint8_t* iter, size_t length);

// Helpers
///////////////////////////////////////////////////////////

static const uint32_t expected_cookie = 0x877d41a;

static void prv_assert_last_event(PhoneEventType subtype,
                                  bool check_cookie, bool check_name_number,
                                  const char *name, const char *number) {
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_PHONE_EVENT);
  cl_assert_equal_i(e.phone.source, PhoneCallSource_PP);
  cl_assert_equal_i(e.phone.type, subtype);
  if (check_cookie) {
    cl_assert_equal_i(e.phone.call_identifier, expected_cookie);
  }
  if (check_name_number) {
    cl_assert_equal_s(e.phone.caller->name, name);
    cl_assert_equal_s(e.phone.caller->number, number);
  }
}

// Tests
///////////////////////////////////////////////////////////

static Transport *s_transport;
static CommSession *s_session;

void test_phone_pp__initialize(void) {
  fake_event_init();
  fake_comm_session_init();
  s_transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
  s_session = fake_transport_set_connected(s_transport, true /* connected */);
  pp_get_phone_state_set_enabled(false);
}

void test_phone_pp__cleanup(void) {
  fake_comm_session_cleanup();
}

void test_phone_pp__incoming_no_caller_id(void) {
  uint8_t pp_msg[] = {0x04, 0x1a, 0xd4, 0x77, 0x08, 0x00, 0x00};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_Incoming, true /* check_cookie */, true /* check_name_number */,
                        "Unknown", NULL);
}

void test_phone_pp__incoming_no_name(void) {
  uint8_t pp_msg[] = {0x04, 0x1a, 0xd4, 0x77, 0x08,
                      0x0d, 0x35, 0x35, 0x35, 0x2D, 0x35, 0x35, 0x35,
                            0x2D, 0x35, 0x35, 0x35, 0x35, 0x00,    // "555-555-5555"
                      0x00};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_Incoming, true /* check_cookie */, true /* check_name_number */,
                        "", "555-555-5555");
}

void test_phone_pp__incoming_no_number(void) {
  uint8_t pp_msg[] = {0x04, 0x1a, 0xd4, 0x77, 0x08,
                      0x00,
                      0x06, 0x42, 0x6F, 0x62, 0x62, 0x79, 0x00};   // "Bobby"
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_Incoming, true /* check_cookie */, true /* check_name_number */,
                        "Bobby", "");
}

void test_phone_pp__incoming(void) {
  uint8_t pp_msg[] = {0x04, 0x1a, 0xd4, 0x77, 0x08,
                      0x0d, 0x35, 0x35, 0x35, 0x2D, 0x35, 0x35, 0x35,
                            0x2D, 0x35, 0x35, 0x35, 0x35, 0x00,    // "555-555-5555"
                      0x06, 0x42, 0x6F, 0x62, 0x62, 0x79, 0x00};   // "Bobby"
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_Incoming, true /* check_cookie */, true /* check_name_number */,
                        "Bobby", "555-555-5555");
}

void test_phone_pp__start(void) {
  uint8_t pp_msg[] = {0x08, 0x1a, 0xd4, 0x77, 0x08};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_Start, true /* check_cookie */, false /* check_name_number */,
                        NULL, NULL);
}

void test_phone_pp__end(void) {
  uint8_t pp_msg[] = {0x09, 0x1a, 0xd4, 0x77, 0x08};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_End, true /* check_cookie */, false /* check_name_number */,
                        NULL, NULL);
}

static void prv_assert_answer_call_sent_cb(uint16_t endpoint_id,
                                           const uint8_t* data, unsigned int data_length) {
  uint8_t expected_answer_msg[] = {0x01, 0x1a, 0xd4, 0x77, 0x08};
  cl_assert_equal_i(sizeof(expected_answer_msg), data_length);
  cl_assert_equal_i(memcmp(expected_answer_msg, data, sizeof(expected_answer_msg)), 0);
}

void test_phone_pp__answer_call(void) {
  fake_transport_set_sent_cb(s_transport, prv_assert_answer_call_sent_cb);
  pp_answer_call(expected_cookie);
  fake_comm_session_process_send_next();
}

static void prv_assert_decline_call_sent_cb(uint16_t endpoint_id,
                                           const uint8_t* data, unsigned int data_length) {
  uint8_t expected_decline_msg[] = {0x02, 0x1a, 0xd4, 0x77, 0x08};
  cl_assert_equal_i(sizeof(expected_decline_msg), data_length);
  cl_assert_equal_i(memcmp(expected_decline_msg, data, sizeof(expected_decline_msg)), 0);
}

void test_phone_pp__decline_call(void) {
  fake_transport_set_sent_cb(s_transport, prv_assert_decline_call_sent_cb);
  pp_decline_call(expected_cookie);
  fake_comm_session_process_send_next();
}

static void prv_assert_get_phone_call_state_sent_cb(uint16_t endpoint_id,
                                                const uint8_t* data, unsigned int data_length) {
  uint8_t expected_request_msg[] = {0x03};
  cl_assert_equal_i(sizeof(expected_request_msg), data_length);
  cl_assert_equal_i(memcmp(expected_request_msg, data, sizeof(expected_request_msg)), 0);
}

void test_phone_pp__get_phone_call_state_request(void) {
  fake_transport_set_sent_cb(s_transport, prv_assert_get_phone_call_state_sent_cb);
  pp_get_phone_state();
  fake_comm_session_process_send_next();
}

void test_phone_pp__get_phone_call_state_response_no_calls(void) {
  pp_get_phone_state_set_enabled(true);
  uint8_t pp_msg[] = {0x83};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_End, false /* check_cookie */, false /* check_name_number */,
                        NULL, NULL);
}

void test_phone_pp__get_phone_call_state_response_one_started_call(void) {
  pp_get_phone_state_set_enabled(true);
  uint8_t pp_msg[] = {0x83, 0x05, 0x08, 0x1a, 0xd4, 0x77, 0x08};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  prv_assert_last_event(PhoneEventType_Start, true /* check_cookie */, false /* check_name_number */,
                        NULL, NULL);
}

void test_phone_pp__get_phone_call_state_response_two_started_calls(void) {
  pp_get_phone_state_set_enabled(true);
  uint8_t pp_msg[] = {0x83,
                      0x05, 0x12, 0x34, 0x45, 0x67, 0x89,
                      0x05, 0x08, 0x1a, 0xd4, 0x77, 0x08};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();
  // TODO: assert the first event as well
  prv_assert_last_event(PhoneEventType_Start, true /* check_cookie */, false /* check_name_number */,
                        NULL, NULL);
}

void test_phone_pp__get_phone_call_state_response_disabled(void) {
  pp_get_phone_state_set_enabled(false);
  uint8_t pp_msg[] = {0x83};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_NULL_EVENT);
}

// PBL-34640 Make sure we don't put an incoming call event to a state response - the incoming call
// state is only used for iOS 8 devices now and just causes trouble
void test_phone_pp__get_phone_call_state_response_incoming(void) {
  pp_get_phone_state_set_enabled(true);
  uint8_t pp_msg[] = {0x83,
                      0x07, 0x04, 0x1a, 0xd4, 0x77, 0x08, 0x00, 0x00};
  phone_protocol_msg_callback(s_session, pp_msg, sizeof(pp_msg));
  fake_system_task_callbacks_invoke_pending();

  // We shouldn't have emitted any event in this case
  cl_assert_equal_i(fake_event_get_count(), 0);
}
