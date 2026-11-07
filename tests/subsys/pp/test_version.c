/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/transport.h>

#include <clar.h>

extern const struct pbl_pp_endpoint pbl_pp_endpoint_0x0011;

// Fakes
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <fake_events.h>
#include <fake_pbl_malloc.h>

static struct pbl_pp_session s_session;

static bool s_session_is_system;
bool pbl_pp_session_is_system(const struct pbl_pp_session *session) {
  return s_session_is_system;
}

static int s_requests_sent;
int pbl_pp_send(struct pbl_pp_session *session, uint16_t endpoint_id, const uint8_t *data,
                size_t length, uint32_t timeout_ms) {
  cl_assert_equal_p(session, &s_session);
  cl_assert_equal_i(endpoint_id, PBL_PP_ENDPOINT_REMOTE_VERSION);
  cl_assert_equal_i(length, 1);
  cl_assert_equal_i(*data, 0x00 /* Request */);
  ++s_requests_sent;
  return 0;
}

static int s_set_capabilities_count;
static uint32_t s_capabilities;
void pbl_pp_session_set_capabilities(struct pbl_pp_session *session, uint32_t capabilities) {
  cl_assert_equal_p(session, &s_session);
  ++s_set_capabilities_count;
  s_capabilities = capabilities;
}

static void (*s_launcher_task_callback)(void *data);
static void *s_launcher_task_callback_data;

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  cl_assert_equal_p(s_launcher_task_callback, nullptr);
  s_launcher_task_callback = callback;
  s_launcher_task_callback_data = data;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

#define HEADER_V1                                              \
  0x00, 0x00, 0x00, 0x00,     /* Deprecated library version */ \
      0x00, 0x00, 0x00, 0x00, /* Deprecated capabilities */    \
      0x00, 0x00, 0x00, 0x02  /* Platform bitfield: Android */

static void prv_receive(const uint8_t *data, size_t length) {
  pbl_pp_endpoint_0x0011.handler(&s_session, data, length);
}

static void prv_receive_v3_response(uint32_t capabilities) {
  const uint8_t response[] = {
    0x01, // Response
    HEADER_V1,
    0x02, // Response version
    3,
    2,
    1,
    capabilities & 0xff,
    (capabilities >> 8) & 0xff,
    (capabilities >> 16) & 0xff,
    (capabilities >> 24) & 0xff,
  };
  prv_receive(response, sizeof(response));
}

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

static const uint32_t EXPECTED_CAPABILITIES = PBL_PP_CAP_APP_MESSAGE_8K | PBL_PP_CAP_VOICE_API;

void test_version__initialize(void) {
  fake_event_init();
  s_session = (struct pbl_pp_session){};
  s_requests_sent = 0;
  s_session_is_system = true;
  s_set_capabilities_count = 0;
  s_capabilities = 0;
  s_launcher_task_callback = nullptr;
  s_launcher_task_callback_data = nullptr;
}

void test_version__cleanup(void) {
  fake_pbl_malloc_clear_tracking();
}

void test_version__endpoint(void) {
  cl_assert_equal_i(pbl_pp_endpoint_0x0011.id, PBL_PP_ENDPOINT_REMOTE_VERSION);
  cl_assert_equal_i(pbl_pp_endpoint_0x0011.access, PBL_PP_ACCESS_ANY);
  cl_assert_equal_i(pbl_pp_endpoint_0x0011.task, PBL_PP_HANDLER_MAIN);
  cl_assert_equal_p(pbl_pp_endpoint_0x0011.receiver, nullptr);
}

void test_version__request_sent_on_kernel_main(void) {
  pbl_pp__version_request(&s_session);
  cl_assert_equal_i(s_requests_sent, 0);
  cl_assert(s_launcher_task_callback);

  s_launcher_task_callback(s_launcher_task_callback_data);
  cl_assert_equal_i(s_requests_sent, 1);
}

void test_version__receive_invalid_msg(void) {
  const uint8_t invalid_msg = 0xff;
  prv_receive(&invalid_msg, sizeof(invalid_msg));
  prv_receive(&invalid_msg, 0);
  cl_assert_equal_i(fake_event_get_count(), 0);
  cl_assert_equal_i(s_set_capabilities_count, 0);
}

void test_version__receive_too_short_response(void) {
  const uint8_t response[] = {0x01, 0x00, 0x00};
  prv_receive(response, sizeof(response));
  cl_assert_equal_i(fake_event_get_count(), 0);
  cl_assert_equal_i(s_set_capabilities_count, 0);
}

void test_version__system_session(void) {
  prv_receive_v3_response(EXPECTED_CAPABILITIES);

  cl_assert_equal_i(fake_event_get_count(), 1);
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_REMOTE_APP_INFO_EVENT);
  cl_assert_equal_i(e.bluetooth.app_info_event.os, PBL_PP_REMOTE_OS_ANDROID);
  cl_assert_equal_i(s_set_capabilities_count, 1);
  cl_assert_equal_i(s_capabilities, EXPECTED_CAPABILITIES);
}

void test_version__app_session(void) {
  s_session_is_system = false;
  prv_receive_v3_response(EXPECTED_CAPABILITIES);

  cl_assert_equal_i(fake_event_get_count(), 0);
  cl_assert_equal_i(s_capabilities, EXPECTED_CAPABILITIES);
}

void test_version__v1_response_has_no_capabilities(void) {
  const uint8_t response[] = {0x01, HEADER_V1};
  s_capabilities = ~0U;
  prv_receive(response, sizeof(response));

  cl_assert_equal_i(s_set_capabilities_count, 1);
  cl_assert_equal_i(s_capabilities, 0);
  cl_assert_equal_i(fake_event_get_count(), 1);
}

void test_version__v2_response_has_no_capabilities(void) {
  const uint8_t response[] = {0x01, HEADER_V1, 0x02, 2, 0, 1};
  s_capabilities = ~0U;
  prv_receive(response, sizeof(response));

  cl_assert_equal_i(s_set_capabilities_count, 1);
  cl_assert_equal_i(s_capabilities, 0);
}
