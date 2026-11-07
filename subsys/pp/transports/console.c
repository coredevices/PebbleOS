/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/console/console.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/transport.h>
#include <pbl/pp/transports/console.h>
#include <pbl/util/math.h>

#include <kernel/events.h>

#define OPCODE_DATA  1U
#define OPCODE_OPEN  2U
#define OPCODE_CLOSE 3U

#define OPCODE_LEN 1U
#define MSS        (PBL_CONSOLE_FRAME_MTU - OPCODE_LEN)

#define TX_RETRY_MS  200U
#define TX_MAX_TRIES 25U

static PBL_MUTEX_DEFINE(s_lock);

static void prv_retry_timer_cb(struct pbl_work *work);

// Under s_lock.
static struct {
  struct pbl_pp_session *session;
  struct pbl_delayed_work tx_timer;
  // Bytes at the head of the send queue in the frame being acknowledged.
  size_t tx_len;
  uint8_t tx_seq;
  uint8_t tx_tries;
} s_transport = {
  .tx_timer = PBL_DELAYED_WORK_INITIALIZER(prv_retry_timer_cb),
};

// Under s_lock.
static uint8_t s_tx_buf[PBL_CONSOLE_FRAME_MTU];

static void prv_send_opcode(uint8_t opcode) {
  pbl_console_frame_send(PBL_CONSOLE_CHANNEL_PP, 0U, &opcode, sizeof(opcode));
}

static void prv_send_frame(void) {
  s_tx_buf[0] = OPCODE_DATA;
  pbl_pp_session_send_queue_copy(s_transport.session, 0U, s_transport.tx_len,
                                 &s_tx_buf[OPCODE_LEN]);
  pbl_console_frame_send(PBL_CONSOLE_CHANNEL_PP, s_transport.tx_seq, s_tx_buf,
                         OPCODE_LEN + s_transport.tx_len);
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_transport.tx_timer, PBL_MSEC(TX_RETRY_MS));
}

static void prv_send_next(struct pbl_pp_session *session) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (session == s_transport.session && s_transport.tx_len == 0U) {
    const size_t len = MIN(pbl_pp_session_send_queue_length(session), MSS);

    if (len > 0U) {
      s_transport.tx_len = len;
      s_transport.tx_seq = (s_transport.tx_seq % UINT8_MAX) + 1U;
      s_transport.tx_tries = 0U;
      prv_send_frame();
    }
  }

  pbl_mutex_unlock(&s_lock);
}

static const struct pbl_pp_transport s_pp_transport = {
  .name = "console",
  .priority = 1U,
  .shared = true,
  .send_next = prv_send_next,
};

void pbl_pp_console_ack(uint8_t seq) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_transport.tx_len > 0U && seq == s_transport.tx_seq) {
    pbl_delayed_work_cancel(&s_transport.tx_timer);
    pbl_pp_session_send_queue_consume(s_transport.session, s_transport.tx_len);
    s_transport.tx_len = 0U;
    pbl_pp_session_send_next(s_transport.session);
  }
  pbl_mutex_unlock(&s_lock);
}

static void prv_put_connection_event(bool connected) {
  PebbleEvent event = {
    .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
    .bluetooth.connection.state = connected ? PebbleBluetoothConnectionEventStateConnected
                                            : PebbleBluetoothConnectionEventStateDisconnected,
  };

  event_put(&event);
}

static void prv_set_connected(bool connected) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if ((s_transport.session != nullptr) != connected) {
    if (connected) {
      s_transport.tx_seq = 0U;
      s_transport.session =
          pbl_pp_session_open(&s_pp_transport, PBL_PP_DESTINATION_HYBRID, nullptr);
      if (s_transport.session != nullptr) {
        pbl_pp_session_set_capabilities(
            s_transport.session, PBL_PP_CAP_RUN_STATE | PBL_PP_CAP_INFINITE_LOG_DUMPING |
                                     PBL_PP_CAP_VOICE_API | PBL_PP_CAP_APP_MESSAGE_8K |
                                     PBL_PP_CAP_WEATHER_APP | PBL_PP_CAP_EXTENDED_NOTIFICATIONS);
        prv_put_connection_event(true);
      }
    } else {
      pbl_delayed_work_cancel(&s_transport.tx_timer);
      s_transport.tx_len = 0U;
      pbl_pp_session_close(s_transport.session);
      s_transport.session = nullptr;
      prv_put_connection_event(false);
    }
  }

  pbl_mutex_unlock(&s_lock);
}

static void prv_retry_timer_cb(struct pbl_work *work) {
  bool give_up = false;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_transport.tx_len > 0U) {
    give_up = (++s_transport.tx_tries >= TX_MAX_TRIES);
    if (!give_up) {
      prv_send_frame();
    }
  }
  pbl_mutex_unlock(&s_lock);

  if (give_up) {
    PBL_LOG_WRN("Host not acknowledging, closing the session");
    prv_set_connected(false);
  }
}

void pbl_pp_console_receive(const void *data, size_t len) {
  const uint8_t *bytes = data;

  if (len < OPCODE_LEN) {
    return;
  }

  switch (bytes[0]) {
    case OPCODE_DATA:
      pbl_mutex_lock(&s_lock, PBL_FOREVER);
      if (s_transport.session != nullptr) {
        pbl_pp_session_receive(s_transport.session, &bytes[OPCODE_LEN], len - OPCODE_LEN);
        pbl_mutex_unlock(&s_lock);
      } else {
        // The host is behind, e.g. the firmware restarted: tell it the session is gone.
        pbl_mutex_unlock(&s_lock);
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
