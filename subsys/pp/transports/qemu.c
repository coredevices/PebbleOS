/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/transport.h>
#include <pbl/util/math.h>

#include <comm/qemu/serial.h>
#include <comm/qemu/serial_private.h>
#include <comm/qemu/settings.h>
#include <kernel/events.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

static const uint32_t READY_LOG_DELAY_MS = 500;

static PBL_MUTEX_DEFINE(s_lock);

// Under s_lock.
static struct pbl_pp_session *s_session;
// Emulated connection state, under s_lock.
static bool s_connected;

static void prv_send_next(struct pbl_pp_session *session) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  size_t remaining = (session == s_session) ? pbl_pp_session_send_queue_length(session) : 0U;

  if (remaining > 0U) {
    const size_t buffer_size = MIN(remaining, QEMU_MAX_DATA_LEN);
    uint8_t *buffer = pbl_malloc_check(buffer_size);

    while (remaining > 0U) {
      const size_t length = MIN(remaining, buffer_size);

      pbl_pp_session_send_queue_copy(session, 0U, length, buffer);
      qemu_serial_send(QemuProtocol_SPP, buffer, length);
      pbl_pp_session_send_queue_consume(session, length);
      remaining -= length;
    }

    pbl_free(buffer);
  }

  pbl_mutex_unlock(&s_lock);
}

static const struct pbl_pp_transport s_transport = {
  .name = "QEMU",
  .priority = 0U,
  .send_next = prv_send_next,
};

static void prv_put_connection_event(bool connected) {
  PebbleEvent event = {
    .type = PBL_BT_PEBBLE_CONNECTION_EVENT,
    .bluetooth.connection.state = connected ? PebbleBluetoothConnectionEventStateConnected
                                            : PebbleBluetoothConnectionEventStateDisconnected,
  };

  event_put(&event);
}

static void prv_put_comm_session_event(bool is_open, bool is_system) {
  PebbleEvent event = {
    .type = PEBBLE_COMM_SESSION_EVENT,
    .bluetooth.comm_session_event.is_open = is_open,
    .bluetooth.comm_session_event.is_system = is_system,
  };

  event_put(&event);
}

void pbl_pp_qemu_set_connected(bool is_connected) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (is_connected && s_session == nullptr) {
    PBL_LOG_DBG("Opening the QEMU session");
    s_session = pbl_pp_session_open(&s_transport, PBL_PP_DESTINATION_HYBRID, nullptr);
    if (s_session == nullptr) {
      PBL_LOG_ERR("Could not open the QEMU session");
    } else {
      pbl_pp_session_set_capabilities(
          s_session, PBL_PP_CAP_RUN_STATE | PBL_PP_CAP_INFINITE_LOG_DUMPING | PBL_PP_CAP_VOICE_API |
                         PBL_PP_CAP_APP_MESSAGE_8K);
      prv_put_connection_event(true);
    }
    s_connected = true;
  } else if (s_connected != is_connected) {
    PBL_LOG_DBG("Emulated session %s", is_connected ? "connecting" : "disconnecting");

    // Only tell apps: the rest of the firmware keeps using the session.
    prv_put_comm_session_event(is_connected, true);
    prv_put_comm_session_event(is_connected, false);
    s_connected = is_connected;
  }

  pbl_mutex_unlock(&s_lock);
}

bool pbl_pp_qemu_is_connected(void) {
  bool connected;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  connected = (s_session != nullptr) && s_connected;
  pbl_mutex_unlock(&s_lock);

  return connected;
}

void pbl_pp_qemu_receive(const uint8_t *data, uint32_t length) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_session != nullptr) {
    pbl_pp_session_receive(s_session, data, length);
  } else {
    PBL_LOG_ERR("Received QEMU serial data, but session not connected!");
  }
  pbl_mutex_unlock(&s_lock);
}

#ifndef CONFIG_BT_HCI_UART
static bool prv_default_connected(void) {
#ifdef CONFIG_QEMU
  return qemu_setting_get(QemuSetting_DefaultConnected);
#elif defined(CONFIG_POSIX_EMSCRIPTEN)
  // In a browser the page may never be a phone: it connects when it is one.
  return false;
#else
  return true;
#endif
}
#endif

void pbl_pp_qemu_start(void) {
#ifndef CONFIG_BT_HCI_UART
  // Open the session synchronously: the host may send a WatchVersionRequest
  // as soon as it sees "Ready for communication".
  if (prv_default_connected()) {
    pbl_pp_qemu_set_connected(true);
  }
#endif
}

static void prv_log_ready(struct pbl_work *work) {
  PBL_LOG_INFO("Ready for communication.");
}

static PBL_DELAYED_WORK_DEFINE(s_ready_log_work, prv_log_ready);

void pbl_pp_qemu_notify_ready(void) {
  // Host tools install on seeing this line, so give the first app time to push its window.
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_ready_log_work, PBL_MSEC(READY_LOG_DELAY_MS));
}

void pbl_pp_qemu_stop(void) {
  pbl_pp_qemu_set_connected(false);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_session != nullptr) {
    pbl_pp_session_close(s_session);
    s_session = nullptr;
    prv_put_connection_event(false);
  }
  pbl_mutex_unlock(&s_lock);
}
