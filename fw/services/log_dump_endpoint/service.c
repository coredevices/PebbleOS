/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/logging/backend.h>
#include <pbl/logging/backend_flash.h>
#include <pbl/logging/logging.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/comm_session/session_send_buffer.h>

#include <kernel/events.h>
#include <kernel/pebble_tasks.h>

PBL_LOG_MODULE_DEFINE(service_log_dump_endpoint, CONFIG_SERVICE_LOG_DUMP_ENDPOINT_LOG_LEVEL);

static const uint16_t ENDPOINT_ID = 2002;

typedef struct PBL_PACKED {
  uint8_t command;
  uint32_t cookie;
} BluetoothHeader;

typedef struct BluetoothDumpLineCallbackData {
  bool in_progress;
  CommSession *comm_session;
  int generation;
  uint32_t cookie;
} BluetoothDumpLineCallbackData;

static BluetoothDumpLineCallbackData s_bt_dump_chunk_callback_data;

static void prv_put_status_event(DebugInfoEventState state) {
  PebbleEvent event = {
    .type = PEBBLE_GATHER_DEBUG_INFO_EVENT,
    .debug_info = {
      .source = DebugInfoSourceFWLogs,
      .state = state,
    },
  };
  event_put(&event);
}

static bool prv_bt_log_dump_line_cb(uint8_t *message, uint32_t total_length) {
  CommSession *session = s_bt_dump_chunk_callback_data.comm_session;

  // keep us sending data quickly
  comm_session_set_responsiveness(s_bt_dump_chunk_callback_data.comm_session,
                                  PBL_BT_CONSUMER_PP_LOG_DUMP, PBL_BT_RESPONSE_TIME_MIN, 5);

  const uint16_t required_length = total_length + 1 + 4;
  SendBuffer *sb = comm_session_send_buffer_begin_write_bulk(session, ENDPOINT_ID, required_length,
                                                             COMM_SESSION_DEFAULT_TIMEOUT);
  if (!sb) {
    PBL_LOG_DBG("Failed to get send buffer");
    return false;
  }

  BluetoothHeader header = {
    .command = 0x80,
    .cookie = s_bt_dump_chunk_callback_data.cookie,
  };
  comm_session_send_buffer_write(sb, (const uint8_t *)&header, sizeof(header));
  comm_session_send_buffer_write(sb, message, total_length);
  comm_session_send_buffer_end_write(sb);
  return true;
}

// Called by pbl_log_flash_dump() when the log has been completely dumped
static void prv_bt_log_dump_completed_cb(bool success) {
  BluetoothHeader header = {.cookie = s_bt_dump_chunk_callback_data.cookie};
  // Send a "no logs" message if the generation did not exist and the remote supports
  // "infinite log dumping"
  CommSession *session = s_bt_dump_chunk_callback_data.comm_session;
  if (!success && comm_session_has_capability(session, CommSessionInfiniteLogDumping)) {
    header.command = 0x82;
    comm_session_send_data(s_bt_dump_chunk_callback_data.comm_session, ENDPOINT_ID,
                           (uint8_t *)&header, sizeof(header), COMM_SESSION_DEFAULT_TIMEOUT);
  } else {
    // Otherwise, just send a "done" message
    header.command = 0x81;
    comm_session_send_data(s_bt_dump_chunk_callback_data.comm_session, ENDPOINT_ID,
                           (uint8_t *)&header, sizeof(header), COMM_SESSION_DEFAULT_TIMEOUT);
  }

  s_bt_dump_chunk_callback_data.in_progress = false;

  // Ok to enter a lower power less responsive state
  comm_session_set_responsiveness(s_bt_dump_chunk_callback_data.comm_session,
                                  PBL_BT_CONSUMER_PP_LOG_DUMP, PBL_BT_RESPONSE_TIME_MAX, 0);
  prv_put_status_event(DebugInfoStateFinished);
}

static void prv_dump(CommSession *session, int generation, uint32_t cookie) {
  PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(PebbleTask_KernelBackground);
  if (s_bt_dump_chunk_callback_data.in_progress) {
    PBL_LOG_ERR("Already in the middle of dumping logs");
    return;
  }

  prv_put_status_event(DebugInfoStateStarted);

  // Temporarily disable logging so we don't log forever.
  pbl_log_backend_disable(PBL_LOG_BACKEND_GET(flash));

  s_bt_dump_chunk_callback_data.in_progress = true;
  s_bt_dump_chunk_callback_data.generation = generation;
  s_bt_dump_chunk_callback_data.comm_session = session;
  s_bt_dump_chunk_callback_data.cookie = cookie;

  pbl_log_flash_dump(s_bt_dump_chunk_callback_data.generation, prv_bt_log_dump_line_cb,
                     prv_bt_log_dump_completed_cb);
  pbl_log_backend_enable(PBL_LOG_BACKEND_GET(flash));
}

void dump_log_protocol_msg_callback(CommSession *session, const uint8_t *data, size_t length) {
  uint32_t cookie;
  int generation = 0;
  if (data[0] == 0x10 || data[0] == 0x11) {
    if (length != 6) {
      PBL_LOG_ERR("Invalid dump log message received -- length %zu", length);
      return;
    }

    generation = data[1];
    cookie = *((uint32_t *)(data + 2));
  } else {
    if (length != 5) {
      PBL_LOG_ERR("Invalid dump log message received -- length %zu", length);
      return;
    }

    cookie = *((uint32_t *)(data + 1));
  }

  switch (*data) {
    case 0x00:
      prv_dump(session, 0, cookie);
      break;
    case 0x01:
      prv_dump(session, 1, cookie);
      break;
    case 0x10:
      prv_dump(session, generation, cookie);
      break;
    case 0x02:
    case 0x03:
    case 0x11:
      break;
  }
}
