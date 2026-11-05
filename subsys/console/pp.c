/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/bluetooth/comm.h>
#include <pbl/logging/logging.h>
#include <pbl/services/comm_session/session_transport.h>
#include <pbl/services/new_timer/new_timer.h>
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

#define TX_RETRY_MS  200U
#define TX_MAX_TRIES 25U

// Under bt_lock.
static struct {
  CommSession *session;
  TimerID tx_timer;
  // Bytes at the head of the send queue in the frame being acknowledged.
  size_t tx_len;
  uint8_t tx_seq;
  uint8_t tx_tries;
} s_transport;

// Under bt_lock.
static uint8_t s_tx_buf[CONSOLE_FRAME_MTU];

static void prv_send_opcode(uint8_t opcode) {
  pbl_console__frame_send(CONSOLE_FRAME_CHANNEL_PP, 0U, &opcode, sizeof(opcode));
}

static void prv_retry_timer_cb(void *unused);

static void prv_send_frame(void) {
  s_tx_buf[0] = OPCODE_DATA;
  comm_session_send_queue_copy(s_transport.session, 0, s_transport.tx_len, &s_tx_buf[OPCODE_LEN]);
  pbl_console__frame_send(CONSOLE_FRAME_CHANNEL_PP, s_transport.tx_seq, s_tx_buf,
                          OPCODE_LEN + s_transport.tx_len);
  new_timer_start(s_transport.tx_timer, TX_RETRY_MS, prv_retry_timer_cb, nullptr, 0);
}

static void prv_send_next(Transport *transport) {
  if (s_transport.tx_len > 0U) {
    return;
  }

  const size_t len = MIN(comm_session_send_queue_get_length(s_transport.session), MSS);

  if (len > 0U) {
    s_transport.tx_len = len;
    s_transport.tx_seq = (s_transport.tx_seq % UINT8_MAX) + 1U;
    s_transport.tx_tries = 0U;
    prv_send_frame();
  }
}

void pbl_console__pp_handle_ack(uint8_t seq) {
  bt_lock();
  if (s_transport.tx_len > 0U && seq == s_transport.tx_seq) {
    new_timer_stop(s_transport.tx_timer);
    comm_session_send_queue_consume(s_transport.session, s_transport.tx_len);
    s_transport.tx_len = 0U;
    comm_session_send_next(s_transport.session);
  }
  bt_unlock();
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
      if (s_transport.tx_timer == TIMER_INVALID_ID) {
        s_transport.tx_timer = new_timer_create();
      }
      s_transport.tx_seq = 0U;
      s_transport.session =
          comm_session_open((Transport *)&s_transport, &s_console_transport_implementation,
                            TransportDestinationHybrid);
    } else {
      new_timer_stop(s_transport.tx_timer);
      s_transport.tx_len = 0U;
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

static void prv_retry_timer_cb(void *unused) {
  bool give_up = false;

  bt_lock();
  if (s_transport.tx_len > 0U) {
    give_up = (++s_transport.tx_tries >= TX_MAX_TRIES);
    if (!give_up) {
      prv_send_frame();
    }
  }
  bt_unlock();

  if (give_up) {
    PBL_LOG_WRN("Host not acknowledging, closing the session");
    prv_set_connected(false);
  }
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
