/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_comm_session_session_analytics Session analytics
 * @ingroup services_comm_session
 * @brief Connectivity analytics hooks for session open and close.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct CommSession CommSession;
/** @endcond */

/** @brief Why a session was closed. */
typedef enum {
  /** Underlying link disconnected. */
  CommSessionCloseReason_UnderlyingDisconnection = 0,
  /** Closed by the phone. */
  CommSessionCloseReason_ClosedRemotely = 1,
  /** Closed by the watch. */
  CommSessionCloseReason_ClosedLocally = 2,
  /** First transport-specific reason. */
  CommSessionCloseReason_TransportSpecificBegin = 100,
  /** Last transport-specific reason. */
  CommSessionCloseReason_TransportSpecificEnd = 255,
} CommSessionCloseReason;

/** @brief Transport type carrying a session. */
typedef enum {
  /** Plain SPP (Android). */
  CommSessionTransportType_PlainSPP = 0,
  /** iAP (iOS). */
  CommSessionTransportType_iAP = 1,
  /** Pebble Protocol over GATT. */
  CommSessionTransportType_PPoGATT = 2,
  /** QEMU emulator transport. */
  CommSessionTransportType_QEMU = 3,
  /** PULSE serial transport. */
  CommSessionTransportType_PULSE = 4,
} CommSessionTransportType;

/**
 * @brief Get the transport type of a session.
 *
 * The caller must hold bt_lock().
 *
 * @param session Session to query.
 * @return Transport type.
 */
CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session);

/**
 * @brief Record a session being opened.
 *
 * Starts the connectivity timers for system sessions.
 *
 * @param session Opened session.
 */
void comm_session_analytics_open_session(CommSession *session);

/**
 * @brief Record a session being closed.
 *
 * Stops the connected-time timer for system sessions.
 *
 * @param session Closed session.
 * @param reason Close reason.
 */
void comm_session_analytics_close_session(CommSession *session, CommSessionCloseReason reason);

/** @} */
