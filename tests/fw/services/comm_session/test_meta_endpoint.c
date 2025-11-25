/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/comm_session/meta_endpoint.h"

#include "clar.h"

// Stubs
///////////////////////////////////////////////////////////

#include "stubs_bt_lock.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_print.h"

// Fakes
///////////////////////////////////////////////////////////

#include "fake_kernel_malloc.h"
#include "fake_session.h"
#include "fake_system_task.h"

static Transport *s_transport;
static CommSession *s_session;

// Helpers
///////////////////////////////////////////////////////////

static void prv_process_and_assert_sent(const uint8_t data[], uint16_t length) {
  const uint16_t meta_endpoint_id = 0;
  fake_system_task_callbacks_invoke_pending();
  fake_comm_session_process_send_next();
  fake_transport_assert_sent(s_transport, 0 /* index */, meta_endpoint_id, data, length);
}

// Tests
///////////////////////////////////////////////////////////

void test_meta_endpoint__initialize(void) {
  fake_kernel_malloc_init();
  fake_kernel_malloc_mark();
  fake_comm_session_init();

  s_transport = fake_transport_create(TransportDestinationSystem, NULL, NULL);
  s_session = fake_transport_set_connected(s_transport, true);
}

void test_meta_endpoint__cleanup(void) {
  // Check for leaks:
  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();

  fake_comm_session_cleanup();
}

void test_meta_endpoint__send_meta_corrupted_message(void) {
  const MetaResponseInfo meta_response_info = {
    .session = s_session,
    .payload = {
      .error_code = MetaResponseCodeCorruptedMessage,
    },
  };
  meta_endpoint_send_response_async(&meta_response_info);

  const uint8_t expected_payload[] = { 0xd0 };
  prv_process_and_assert_sent(expected_payload, sizeof(expected_payload));
}

void test_meta_endpoint__send_meta_disallowed_message(void) {
  const MetaResponseInfo meta_response_info = {
    .session = s_session,
    .payload = {
      .error_code = MetaResponseCodeDisallowed,
      .endpoint_id = 0xabcd,
    },
  };
  meta_endpoint_send_response_async(&meta_response_info);

  const uint8_t expected_payload[] = { 0xdd, 0xab, 0xcd };
  prv_process_and_assert_sent(expected_payload, sizeof(expected_payload));
}

void test_meta_endpoint__send_meta_unhandled_message(void) {
  const MetaResponseInfo meta_response_info = {
    .session = s_session,
    .payload = {
      .error_code = MetaResponseCodeUnhandled,
      .endpoint_id = 0x1234,
    },
  };
  meta_endpoint_send_response_async(&meta_response_info);

  const uint8_t expected_payload[] = { 0xdc, 0x12, 0x34 };
  prv_process_and_assert_sent(expected_payload, sizeof(expected_payload));
}

