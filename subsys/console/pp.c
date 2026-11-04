/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/comm.h>
#include <pbl/logging/logging.h>
#include <pbl/services/comm_session/session_transport.h>
#include <pbl/util/math.h>

#include <comm/bt_lock.h>
#include <console/console_private.h>
#include <kernel/event_loop.h>
#include <kernel/events.h>

#define OPCODE_DATA  1U
#define OPCODE_OPEN  2U
#define OPCODE_CLOSE 3U

#define OPCODE_LEN 1U
#define MSS        (CONSOLE_FRAME_MTU - OPCODE_LEN)

// Only the session's handle is needed to identify the transport.
static struct {
  CommSession *session;
} s_transport;

// Under bt_lock.
static uint8_t s_tx_buf[CONSOLE_FRAME_MTU];

static void prv_send_opcode(uint8_t opcode) {
  pbl_console__frame_send(CONSOLE_FRAME_CHANNEL_PP, &opcode, sizeof(opcode));
}

static void prv_send_next(Transport *transport) {
  CommSession *session = s_transport.session;
  size_t remaining = comm_session_send_queue_get_length(session);

  while (remaining > 0U) {
    const size_t len = MIN(remaining, MSS);

    s_tx_buf[0] = OPCODE_DATA;
    comm_session_send_queue_copy(session, 0, len, &s_tx_buf[OPCODE_LEN]);
    pbl_console__frame_send(CONSOLE_FRAME_CHANNEL_PP, s_tx_buf, OPCODE_LEN + len);
    comm_session_send_queue_consume(session, len);
    remaining -= len;
  }
}

static void prv_reset(Transport *transport) {
  PBL_LOG_WRN("Unimplemented");
}

static void prv_granted_cb(void *ctx) {
  pbl_bt_responsiveness_granted_cb_t granted_handler = ctx;
  granted_handler();
}

static void prv_set_connection_responsiveness(Transport *transport, enum pbl_bt_consumer consumer,
                                              enum pbl_bt_response_time_state state,
                                              uint16_t max_period_secs,
                                              pbl_bt_responsiveness_granted_cb_t granted_handler) {
  if (granted_handler) {
    launcher_task_add_callback(prv_granted_cb, granted_handler);
  }
}

static CommSessionTransportType prv_get_type(struct Transport *transport) {
  return CommSessionTransportType_Console;
}

static void prv_send_job(void *data) {
  pbl_bt_run_send_next_job(data, true);
}

static bool prv_schedule_send_next_job(CommSession *session) {
  launcher_task_add_callback(prv_send_job, session);
  return true;
}

static bool prv_is_current_task_schedule_task(struct Transport *transport) {
  return launcher_task_is_current_task();
}

//! Defined in session.c
extern void comm_session_set_capabilities(CommSession *session,
                                          CommSessionCapability capability_flags);

static const TransportImplementation s_console_transport_implementation = {
  .send_next = prv_send_next,
  .reset = prv_reset,
  .set_connection_responsiveness = prv_set_connection_responsiveness,
  .get_type = prv_get_type,
  .schedule = prv_schedule_send_next_job,
  .is_current_task_schedule_task = prv_is_current_task_schedule_task,
};

static void prv_set_connected(bool connected) {
  bt_lock();

  if ((s_transport.session != nullptr) != connected) {
    if (connected) {
      s_transport.session =
          comm_session_open((Transport *)&s_transport, &s_console_transport_implementation,
                            TransportDestinationHybrid);
    } else {
      comm_session_close(s_transport.session, CommSessionCloseReason_UnderlyingDisconnection);
      s_transport.session = nullptr;
    }

    if (s_transport.session != nullptr) {
      comm_session_set_capabilities(
          s_transport.session, CommSessionRunState | CommSessionInfiniteLogDumping |
                                   CommSessionVoiceApiSupport | CommSessionAppMessage8kSupport |
                                   CommSessionWeatherAppSupport |
                                   CommSessionExtendedNotificationService);
    }

    if (s_transport.session != nullptr || !connected) {
      PebbleEvent e = {
        .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
        .bluetooth = {
          .connection = {
            .state = connected ? PebbleBluetoothConnectionEventStateConnected
                               : PebbleBluetoothConnectionEventStateDisconnected
          }
        }
      };
      event_put(&e);
    }
  }

  bt_unlock();
}

void pbl_console__pp_handle_frame(const void *data, size_t len) {
  const uint8_t *bytes = data;

  if (len < OPCODE_LEN) {
    return;
  }

  switch (bytes[0]) {
    case OPCODE_DATA:
      bt_lock();
      if (s_transport.session != nullptr) {
        comm_session_receive_router_write(s_transport.session, &bytes[OPCODE_LEN],
                                          len - OPCODE_LEN);
        bt_unlock();
      } else {
        // The host is behind, e.g. the firmware restarted: tell it the session is gone.
        bt_unlock();
        prv_send_opcode(OPCODE_CLOSE);
      }
      break;
    case OPCODE_OPEN:
      prv_set_connected(true);
      prv_send_opcode(OPCODE_OPEN);
      break;
    case OPCODE_CLOSE:
      prv_set_connected(false);
      prv_send_opcode(OPCODE_CLOSE);
      break;
    default:
      break;
  }
}
