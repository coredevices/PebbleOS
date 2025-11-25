/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef struct CommSession CommSession;

typedef enum {
  CommSessionCloseReason_UnderlyingDisconnection = 0,
  CommSessionCloseReason_ClosedRemotely = 1,
  CommSessionCloseReason_ClosedLocally = 2,
  CommSessionCloseReason_TransportSpecificBegin = 100,
  CommSessionCloseReason_TransportSpecificEnd = 255,
} CommSessionCloseReason;

typedef enum {
  CommSessionTransportType_PlainSPP = 0,
  CommSessionTransportType_iAP = 1,
  CommSessionTransportType_PPoGATT = 2,
  CommSessionTransportType_QEMU = 3,
  CommSessionTransportType_PULSE = 4,
} CommSessionTransportType;

//! Assumes bt_lock() is held by the caller.
CommSessionTransportType comm_session_analytics_get_transport_type(CommSession *session);

void comm_session_analytics_open_session(CommSession *session);

void comm_session_analytics_close_session(CommSession *session, CommSessionCloseReason reason);

void comm_session_analytics_inc_bytes_sent(CommSession *session, uint16_t length);

void comm_session_analytics_inc_bytes_received(CommSession *session, uint16_t length);
