/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session.h"

bool comm_session_has_capability(CommSession *session, CommSessionCapability capability){
  return true;
}

CommSession *comm_session_get_system_session(void) {
  // Don't return NULL, lots of code paths expect a valid session
  return (CommSession *) 1;
}

bool comm_session_is_system(CommSession *session) {
  return false;
}

CommSession *comm_session_get_by_type(CommSessionType type) {
  return NULL;
}

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id,
                            const uint8_t* data, size_t length, uint32_t timeout_ms) {
  return true;
}
