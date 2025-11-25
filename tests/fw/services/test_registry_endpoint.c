/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_watch_info.h"
#include "services/common/comm_session/session.h"
#include "services/common/system_task.h"

#include <string.h>

extern void factory_registry_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t length_bytes);

// Fakes
/////////////////////////////////////////////////////////////////////

static int s_send_data_count = 0;

static uint8_t* s_expected_response = NULL;
static unsigned int s_expected_response_length = 0;

bool comm_session_send_data(CommSession* comm_session_ref, uint16_t endpoint_id,
                            const uint8_t* data, size_t length, uint32_t timeout_ms) {

  ++s_send_data_count;

  cl_assert_equal_i(endpoint_id, 5001);
  cl_assert_equal_i(length, s_expected_response_length);
  for (int i = 0; i < length; ++i) {
    cl_assert_equal_i(data[i], s_expected_response[i]);
  }

  return true;
}

static int s_watch_color = 0x1;

WatchInfoColor mfg_info_get_watch_color(void) {
  return s_watch_color;
}

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

CommSession* comm_session_get_system_session(void) {
  return NULL;
}

// Tests
/////////////////////////////////////////////////////////////////////

void test_registry_endpoint__initialize(void) {
  s_send_data_count = 0;
  s_expected_response = NULL;
  s_expected_response_length = 0;

  s_watch_color = 0x1;
}

void test_registry_endpoint__pass(void) {
  uint8_t message[] = { 0x0, 0x9, 'm', 'f', 'g', '_', 'c', 'o', 'l', 'o', 'r' };

  uint8_t expected_response[] = { 0x01, 0x04, 0x0, 0x0, 0x0, 0x1 };
  s_expected_response = expected_response;
  s_expected_response_length = sizeof(expected_response);

  factory_registry_protocol_msg_callback(NULL, message, sizeof(message));

  cl_assert_equal_i(s_send_data_count, 1);
}

void test_registry_endpoint__fail_write(void) {
  // Write mfg_color to 4 bytes of zeros
  uint8_t message[] = { 0x2, 0x9, 0x4, 'm', 'f', 'g', '_', 'c', 'o', 'l', 'o', 'r', 0, 0, 0, 0 };

  uint8_t expected_response[] = { 0xff };
  s_expected_response = expected_response;
  s_expected_response_length = sizeof(expected_response);

  factory_registry_protocol_msg_callback(NULL, message, sizeof(message));

  cl_assert_equal_i(s_send_data_count, 1);
}

void test_registry_endpoint__fail_read_other(void) {
  uint8_t message[] = { 0x0, 0x7, 'm', 'f', 'g', '_', 'x', 'x', 'x', };

  uint8_t expected_response[] = { 0xff };
  s_expected_response = expected_response;
  s_expected_response_length = sizeof(expected_response);

  factory_registry_protocol_msg_callback(NULL, message, sizeof(message));

  cl_assert_equal_i(s_send_data_count, 1);
}
