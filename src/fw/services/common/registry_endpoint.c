/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/comm_session/session.h"
#include "mfg/mfg_info.h"

#include <string.h>

#define MFG_COLOR_KEY "mfg_color"

#define FACTORY_REGISTRY_ENDPOINT 5001

static void prv_send_response(const uint8_t *data, unsigned int length) {
  comm_session_send_data(comm_session_get_system_session(), FACTORY_REGISTRY_ENDPOINT,
                         data, length, COMM_SESSION_DEFAULT_TIMEOUT);
}

static void prv_send_color_response(void) {
  const uint8_t response[] = { 0x01, 0x04, 0x0, 0x0, 0x0, mfg_info_get_watch_color() };
  prv_send_response(response, sizeof(response));
}

static void prv_send_fail_response(void) {
  const uint8_t error_response = 0xff;
  prv_send_response(&error_response, sizeof(error_response));
}

void factory_registry_protocol_msg_callback(CommSession *session, const uint8_t* data, size_t length_bytes) {
  // Expected message is 0x0 (Read), 0x09 (Length of key), followed by the string "mfg_color".
  // All other messages just get an error response

  if (length_bytes == 1 + 1 + strlen(MFG_COLOR_KEY) &&
      data[0] == 0x0 &&
      data[1] == strlen(MFG_COLOR_KEY) &&
      memcmp(data + 2, MFG_COLOR_KEY, strlen(MFG_COLOR_KEY)) == 0) {

    prv_send_color_response();
    return;
  }

  prv_send_fail_response();
}

