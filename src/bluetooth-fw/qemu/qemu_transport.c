/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"

#include "services/common/comm_session/session_transport.h"

#include "system/passert.h"
#include "system/logging.h"

#include "comm/bt_lock.h"

#include "drivers/qemu/qemu_serial.h"
#include "drivers/qemu/qemu_serial_private.h"

#include "util/math.h"

#include <bluetooth/qemu_transport.h>

#include <string.h>

typedef struct {
  CommSession *session;
} QemuTransport;

// -----------------------------------------------------------------------------------------
// Static Variables (protected by bt_lock)

//! The CommSession that the QEMU transport is managing.
//! Currently there's only one for the System session.
static QemuTransport s_transport;

// -----------------------------------------------------------------------------------------
// bt_lock() is held by caller
static void prv_send_next(Transport *transport) {
  CommSession *session = s_transport.session;
  PBL_ASSERTN(session);
  size_t bytes_remaining = comm_session_send_queue_get_length(session);
  if (bytes_remaining == 0) {
    return;
  }

  const size_t temp_buffer_size = MIN(bytes_remaining, QEMU_MAX_DATA_LEN);
  uint8_t *temp_buffer = kernel_malloc_check(temp_buffer_size);
  while (bytes_remaining) {
    const size_t bytes_to_copy = MIN(bytes_remaining, temp_buffer_size);
    comm_session_send_queue_copy(session, 0 /* start_offset */, bytes_to_copy, temp_buffer);
    qemu_serial_send(QemuProtocol_SPP, temp_buffer, bytes_to_copy);
    comm_session_send_queue_consume(session, bytes_to_copy);
    bytes_remaining -= bytes_to_copy;
  }
  kernel_free(temp_buffer);
}

// -----------------------------------------------------------------------------------------
// bt_lock() is held by caller
static void prv_reset(Transport *transport) {
  PBL_LOG(LOG_LEVEL_INFO, "Unimplemented");
}

static void prv_granted_kernel_main_cb(void *ctx) {
  ResponsivenessGrantedHandler granted_handler = ctx;
  granted_handler();
}

static void prv_set_connection_responsiveness(
    Transport *transport, BtConsumer consumer, ResponseTimeState state, uint16_t max_period_secs,
    ResponsivenessGrantedHandler granted_handler) {
  PBL_LOG(LOG_LEVEL_INFO, "Consumer %d: requesting change to %d for %" PRIu16 "seconds",
          consumer, state, max_period_secs);

  // it's qemu, our request to bump the speed is always granted!
  if (granted_handler) {
    launcher_task_add_callback(prv_granted_kernel_main_cb, granted_handler);
  }
}

static CommSessionTransportType prv_get_type(struct Transport *transport) {
  return CommSessionTransportType_QEMU;
}

//! Defined in session.c
extern void comm_session_set_capabilities(
    CommSession *session, CommSessionCapability capability_flags);

// -----------------------------------------------------------------------------------------
void qemu_transport_set_connected(bool is_connected) {
  bt_lock();

  const bool transport_is_connected = (s_transport.session);
  if (transport_is_connected == is_connected) {
    bt_unlock();
    return;
  }

  static const TransportImplementation s_qemu_transport_implementation = {
    .send_next = prv_send_next,
    .reset = prv_reset,
    .set_connection_responsiveness = prv_set_connection_responsiveness,
    .get_type = prv_get_type,
  };

  bool send_event = true;

  if (is_connected) {
    s_transport.session = comm_session_open((Transport *) &s_transport,
                                            &s_qemu_transport_implementation,
                                            TransportDestinationHybrid);
    if (!s_transport.session) {
      PBL_LOG(LOG_LEVEL_ERROR, "CommSession couldn't be opened");
      send_event = false;
    }

    // Give it the appropriate capabilities
    const CommSessionCapability capabilities = CommSessionRunState |
                                               CommSessionInfiniteLogDumping |
                                               CommSessionVoiceApiSupport |
                                               CommSessionAppMessage8kSupport;
    comm_session_set_capabilities(s_transport.session, capabilities);
  } else {
    comm_session_close(s_transport.session, CommSessionCloseReason_UnderlyingDisconnection);
    s_transport.session = NULL;
  }

  if (send_event) {
    PebbleEvent e = {
      .type = PEBBLE_BT_CONNECTION_EVENT,
      .bluetooth = {
        .connection = {
          .state = (s_transport.session) ? PebbleBluetoothConnectionEventStateConnected
          : PebbleBluetoothConnectionEventStateDisconnected
        }
      }
    };
    event_put(&e);
  }

  bt_unlock();
}

// -----------------------------------------------------------------------------------------
bool qemu_transport_is_connected(void) {
  return (s_transport.session != NULL);
}

// -----------------------------------------------------------------------------------------
// Handle incoming Qemu-SPP packet data
void qemu_transport_handle_received_data(const uint8_t *data, uint32_t length) {
  bt_lock();
  if (!s_transport.session) {
    PBL_LOG(LOG_LEVEL_ERROR, "Received QEMU serial data, but session not connected!");
    goto unlock;
  }
  comm_session_receive_router_write(s_transport.session, data, length);
unlock:
  bt_unlock();
}
