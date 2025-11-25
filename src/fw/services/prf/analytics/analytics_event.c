/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/uuid.h"
#include "services/common/analytics/analytics_event.h"
#include "services/common/comm_session/session_internal.h"

//! Stub for PRF

void analytics_event_crash(uint8_t crash_code, uint32_t link_register) {
}

void analytics_event_local_bt_disconnect(uint16_t handle, uint32_t lr) {
}

typedef struct CommSession CommSession;
void analytics_event_put_byte_stats(
    CommSession *session, bool crc_good, uint8_t type,
    uint32_t bytes_transferred, uint32_t elapsed_time_ms,
    uint32_t conn_events, uint32_t sync_errors, uint32_t skip_errors, uint32_t other_errors) {
}

void analytics_event_PPoGATT_disconnect(time_t timestamp, bool successful_reconnect) {
}

void analytics_event_get_bytes_stats(
  CommSession *session, uint8_t type, uint32_t bytes_transferred, uint32_t elapsed_time_ms,
  uint32_t conn_events, uint32_t sync_errors, uint32_t skip_errors, uint32_t other_errors) {
}
