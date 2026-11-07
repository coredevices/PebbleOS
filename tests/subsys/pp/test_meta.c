/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <clar.h>

extern const struct pbl_pp_endpoint pbl_pp_endpoint_0x0000;

// Fakes
///////////////////////////////////////////////////////////

#include <fake_kernel_malloc.h>
#include <fake_session.h>
#include <fake_sys_work.h>

static struct fake_transport *s_transport;
static struct pbl_pp_session *s_session;

// Helpers
///////////////////////////////////////////////////////////

static void prv_process_and_assert_sent(const uint8_t data[], uint16_t length) {
  fake_sys_work_callbacks_invoke_pending();
  fake_comm_session_process_send_next();
  fake_transport_assert_sent(s_transport, 0 /* index */, PBL_PP_ENDPOINT_META, data, length);
}

// Tests
///////////////////////////////////////////////////////////

void test_meta__initialize(void) {
  fake_kernel_malloc_init();
  fake_kernel_malloc_mark();
  fake_comm_session_init();

  s_transport = fake_transport_create(PBL_PP_DESTINATION_SYSTEM, nullptr, nullptr);
  s_session = fake_transport_set_connected(s_transport, true);
}

void test_meta__cleanup(void) {
  fake_comm_session_cleanup();

  fake_kernel_malloc_mark_assert_equal();
  fake_kernel_malloc_deinit();
}

void test_meta__endpoint(void) {
  cl_assert_equal_i(pbl_pp_endpoint_0x0000.id, PBL_PP_ENDPOINT_META);
  cl_assert_equal_i(pbl_pp_endpoint_0x0000.access, PBL_PP_ACCESS_ANY);
  cl_assert(pbl_pp_endpoint_0x0000.handler);

  const uint8_t data[] = {0x01};
  pbl_pp_endpoint_0x0000.handler(s_session, data, sizeof(data));
  fake_comm_session_process_send_next();
  fake_transport_assert_nothing_sent(s_transport);
}

void test_meta__send_corrupted_message(void) {
  pbl_pp__meta_send_error(s_session, PBL_PP__META_CORRUPTED, 0x1234);

  const uint8_t expected_payload[] = {0xd0};
  prv_process_and_assert_sent(expected_payload, sizeof(expected_payload));
}

void test_meta__send_disallowed_message(void) {
  pbl_pp__meta_send_error(s_session, PBL_PP__META_DISALLOWED, 0xabcd);

  const uint8_t expected_payload[] = {0xdd, 0xab, 0xcd};
  prv_process_and_assert_sent(expected_payload, sizeof(expected_payload));
}

void test_meta__send_unhandled_message(void) {
  pbl_pp__meta_send_error(s_session, PBL_PP__META_UNHANDLED, 0x1234);

  const uint8_t expected_payload[] = {0xdc, 0x12, 0x34};
  prv_process_and_assert_sent(expected_payload, sizeof(expected_payload));
}
