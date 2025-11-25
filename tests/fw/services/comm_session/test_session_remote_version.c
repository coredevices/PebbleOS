/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/common/comm_session/session_internal.h"
#include "services/common/comm_session/session_remote_os.h"
#include "services/common/comm_session/session_remote_version.h"
#include "util/net.h"

static CommSession s_session;

extern void session_remote_version_protocol_msg_callback(CommSession *session,
                                                         const uint8_t *data, size_t length);

// Fakes & Stubs
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "fake_pbl_malloc.h"
#include "fake_events.h"
#include "fake_new_timer.h"

#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_rtc.h"

void bt_driver_reconnect_notify_platform_bitfield(uint32_t p) {}

static bool s_session_is_system;
bool comm_session_is_system(CommSession *session) {
  return s_session_is_system;
}

static bool s_session_is_valid;
bool comm_session_is_valid(const CommSession *session) {
  return (session == &s_session) && s_session_is_valid;
}

static bool s_data_sent;
bool comm_session_send_data(CommSession *session, uint16_t endpoint_id,
                            const uint8_t* data, size_t length, uint32_t timeout_ms) {
  // The request is just a single 0x00 byte to endpoint 0x11:
  cl_assert_equal_i(endpoint_id, 0x11);
  cl_assert_equal_i(length, 1);
  cl_assert_equal_i(*data, 0x00 /* Command ID 'Request' */);
  s_data_sent = true;
  return true;
}

CommSessionCapability s_capability_flags;
void comm_session_set_capabilities(CommSession *session, CommSessionCapability capability_flags) {
  cl_assert_equal_p(session, &s_session);
  s_capability_flags = capability_flags;
}

BTBondingID bt_persistent_storage_store_bt_classic_pairing(BTDeviceAddress *address, SM128BitKey *link_key,
                                                    char *name, uint8_t *platform_bits) {
  return 1;
}

// Helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

static void (*s_launcher_task_callback)(void *data);
static void *s_launcher_task_callback_data;

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  // Simple fake, can only handle one call
  cl_assert_equal_p(s_launcher_task_callback, NULL);

  s_launcher_task_callback = callback;
  s_launcher_task_callback_data = data;
}

static void prv_process_and_assert_sent_request_data(bool expect_request_data_sent) {
  s_data_sent = false;
  s_launcher_task_callback(s_launcher_task_callback_data);
  cl_assert_equal_b(s_data_sent, expect_request_data_sent);
}

static void prv_receive_v3_response(uint8_t major, uint8_t minor, uint8_t bugfix,
                                    CommSessionCapability protocol_capabilities) {
  union {
    CommSessionCapability protocol_capabilities;
    uint8_t byte[8];  // Little-endian!
  } capabilities;
  capabilities.protocol_capabilities = protocol_capabilities;

  uint8_t response_data[] = {
    0x01,                   // Command ID 'Response'
    0x00, 0x00, 0x00, 0x00, // Deprecated library version
    0x00, 0x00, 0x00, 0x00, // Deprecated capabilities
    0x00, 0x00, 0x00, 0x00, // Platform (OS) bitfield
    0x02,                   // Response version
    major, minor, bugfix,
    capabilities.byte[0],
    capabilities.byte[1],
    capabilities.byte[2],
    capabilities.byte[3],
    capabilities.byte[4],
    capabilities.byte[5],
    capabilities.byte[7],
  };
  session_remote_version_protocol_msg_callback(&s_session, response_data, sizeof(response_data));
}

// Tests
////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_ATTEMPTS (3)

void test_session_remote_version__initialize(void) {
  fake_event_init();
  s_session = (CommSession) {};
  s_data_sent = false;
  s_session_is_valid = true;
  s_session_is_system = true;
  s_capability_flags = 0;
  session_remote_version_start_requests(&s_session);
}

void test_session_remote_version__cleanup(void) {
  s_launcher_task_callback = NULL;
  s_launcher_task_callback_data = NULL;

  fake_pbl_malloc_clear_tracking();
}

void test_session_remote_version__receive_invalid_msg(void) {
  uint8_t invalid_msg = 0xff;
  session_remote_version_protocol_msg_callback(&s_session, &invalid_msg, sizeof(invalid_msg));
  cl_assert_equal_i(fake_event_get_count(), 0);
}

static const CommSessionCapability s_expected_capabilities =
    (CommSessionAppMessage8kSupport |
     CommSessionVoiceApiSupport);

void test_session_remote_version__system_session(void) {
  s_session_is_system = true;
  prv_receive_v3_response(3, 2, 1, s_expected_capabilities);
  // Triggers PEBBLE_REMOTE_APP_INFO_EVENT:
  cl_assert_equal_i(fake_event_get_count(), 1);
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_REMOTE_APP_INFO_EVENT);
  cl_assert_equal_i(s_capability_flags, s_expected_capabilities);
}

void test_session_remote_version__app_session(void) {
  s_session_is_system = false;
  prv_receive_v3_response(3, 2, 1, s_expected_capabilities);
  // Triggers no event:
  cl_assert_equal_i(fake_event_get_count(), 0);
  cl_assert_equal_i(s_capability_flags, s_expected_capabilities);
}
