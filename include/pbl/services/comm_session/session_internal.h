/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "session_receive_router.h"
#include "session_transport.h"

#include <pbl/drivers/rtc.h>
#include <pbl/services/regular_timer.h>
#include <pbl/util/list.h>

/**
 * @defgroup services_comm_session_session_internal Session internals
 * @ingroup services_comm_session
 * @brief Session state, for the session module and its transports.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct SessionSendQueueJob SessionSendQueueJob;
/** @endcond */

/**
 * @brief Pebble Protocol communication session.
 *
 * With iAP, the Pebble app has one session and third party apps share another. With PPoGATT,
 * the Pebble app and each third party app have their own session.
 */
typedef struct CommSession {
  /** Node in the list of open sessions. */
  ListNode node;

  /** Transport carrying the session (SPP, iAP, PPoGATT, QEMU or PULSE). */
  Transport *transport;

  /** Callbacks into the transport. */
  const TransportImplementation *transport_imp;

  /** True if a callback to the transport's send_next() is already scheduled. */
  bool is_send_next_call_pending;

  /** Whether the session serves the system, an app, or both. */
  TransportDestination destination;

  /** Capabilities announced by the phone. */
  CommSessionCapability protocol_capabilities;

  /** Head of the send queue. */
  SessionSendQueueJob *send_queue_head;

  /** Inbound message parser state. */
  ReceiveRouter recv_router;

  /** RTC ticks when the session was opened. */
  RtcTicks open_ticks;
} CommSession;

/** @} */
