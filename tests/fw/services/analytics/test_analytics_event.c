/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/analytics/analytics_event.h"
#include "services/common/analytics/analytics_logging.h"
#include "services/common/comm_session/session.h"
#include "services/common/comm_session/session_analytics.h"

#include <util/uuid.h>

#include <clar.h>

//
// Stubs
//
#include "stubs_bt_lock.h"
#include "stubs_passert.h"

CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session) {
  return CommSessionTransportType_PlainSPP;
}

bool comm_session_is_valid(const CommSession *session) {
  return false;
}

typedef struct GAPLEConnection GAPLEConnection;

GAPLEConnection *gap_le_connection_get_gateway(void) {
  return NULL;
}

const PebbleProcessMd *launcher_menu_app_get_app_info(void) {
  return NULL;
}

void sys_analytics_logging_log_event(AnalyticsEventBlob *event_blob) {
}

bool sys_process_manager_get_current_process_uuid(Uuid *uuid_out) {
  return false;
}

//
// Fakes & helpers
//

#define TEST_EVENT_BLOB_VERSION (0)
#define TEST_EVENT_TIMESTAMP (0)

static AnalyticsEventBlob s_last_blob;
void analytics_logging_log_event(AnalyticsEventBlob *event_blob) {
  s_last_blob = *event_blob;
  s_last_blob.version = TEST_EVENT_BLOB_VERSION;
  s_last_blob.timestamp = TEST_EVENT_TIMESTAMP;
  s_last_blob.kind = ANALYTICS_BLOB_KIND_EVENT;
}

#define cl_assert_equal_last_blob(b) \
  cl_assert_equal_m(&s_last_blob, &b, sizeof(s_last_blob));

//
// Tests
//
void test_analytics_event__initialization(void) {
  s_last_blob = (AnalyticsEventBlob) {};
}

void test_analytics_event__cleanup(void) {

}

void test_analytics_event__analytics_event_app_crash(void) {
  const Uuid app_uuid = UuidMake(0xBE, 0x85, 0x14, 0x68, 0x70, 0x21, 0x43, 0xC6,
                                 0xAB, 0x44, 0xB8, 0x36, 0xFC, 0xD0, 0x33, 0x04);
  const uint32_t pc = 0x8888888;
  const uint32_t lr = 0x2222222;
  const uint8_t build_id[BUILD_ID_EXPECTED_LEN] = {
    0x53, 0x98, 0xB6, 0x7E, 0x98, 0xA2, 0x44, 0x35, 0x67, 0x9B,
    0xA4, 0xB0, 0x08, 0x95, 0xB8, 0x8F, 0x14, 0xDA, 0x5A, 0x11,
  };

  AnalyticsEventBlob expected_blob = {
    .kind = ANALYTICS_BLOB_KIND_EVENT,
    .version = TEST_EVENT_BLOB_VERSION,
    .timestamp = TEST_EVENT_TIMESTAMP,
    .event = AnalyticsEvent_AppCrash,
    .app_crash_report = {
      .uuid = app_uuid,
      .pc = pc,
      .lr = lr,
      .build_id_slice = {
        build_id[0], build_id[1], build_id[2], build_id[3],
      },
    },
  };

  // Non-Rocky.js app:
  analytics_event_app_crash(&app_uuid, pc, lr, build_id, false /* is_rocky_app */);
  expected_blob.event = AnalyticsEvent_AppCrash;
  cl_assert_equal_last_blob(expected_blob);

  // Rocky.js app:
  expected_blob.event = AnalyticsEvent_RockyAppCrash;
  analytics_event_app_crash(&app_uuid, pc, lr, build_id, true /* is_rocky_app */);
  cl_assert_equal_last_blob(expected_blob);
}
