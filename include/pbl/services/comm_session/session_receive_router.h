/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/comm_session/protocol.h>

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup services_comm_session_session_receive_router Receive router
 * @ingroup services_comm_session
 * @brief Inbound message parsing and dispatch to endpoint handlers.
 *
 * Endpoints are declared in @c fw/services/comm_session/protocol_endpoints_table.json.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct CommSession CommSession;
/** @endcond */

/**
 * @brief Pebble Protocol endpoint handler.
 *
 * @param session Session the message arrived on.
 * @param data Message payload.
 * @param length Length of @p data in bytes.
 */
typedef void (*PebbleProtocolEndpointHandler)(CommSession *session, const uint8_t *data,
                                              size_t length);

/** @brief Who may send messages to an endpoint. */
typedef enum {
  /** Third party phone apps. */
  PebbleProtocolAccessPublic = 1 << 0,
  /** The Pebble mobile app. */
  PebbleProtocolAccessPrivate = 1 << 1,
  /** Anyone. */
  PebbleProtocolAccessAny = ~0,
  /** No one. */
  PebbleProtocolAccessNone = 0,
} PebbleProtocolAccess;

/** @brief Message receiver implementation, see struct ReceiverImplementation. */
typedef struct ReceiverImplementation ReceiverImplementation;

/** @brief A single Pebble Protocol endpoint. */
typedef struct PebbleProtocolEndpoint {
  /** Endpoint ID. */
  uint16_t endpoint_id;
  /** Handler for complete messages. */
  PebbleProtocolEndpointHandler handler;
  /** Senders allowed to use the endpoint. */
  PebbleProtocolAccess access_mask;
  /** Receiver that buffers the endpoint's messages. */
  const ReceiverImplementation *receiver_imp;
  /** Optional receiver configuration, passed to the receiver. */
  const void *receiver_opt;
} PebbleProtocolEndpoint;

/**
 * @brief Opaque context of the message currently being received.
 *
 * Its contents are up to the @ref ReceiverImplementation. Messages are not interleaved within
 * one session, so each session has at most one Receiver at a time.
 */
typedef struct Receiver Receiver;

/**
 * @brief Buffers inbound message payloads and schedules endpoint handlers.
 *
 * A receiver creates a Receiver context (prepare), buffers payload data (write) and schedules
 * the endpoint handler (finish). It can be specific to one endpoint (Put Bytes needs a large
 * buffer) or shared by endpoints with similar buffering needs.
 *
 * Several sessions may be writing partial messages concurrently; the receiver must handle
 * that. All callbacks are mandatory.
 */
typedef struct ReceiverImplementation {
  /**
   * @brief Prepare a Receiver context for a new message.
   *
   * The endpoint's @c receiver_opt is available through @p endpoint.
   *
   * @param session Session receiving the message.
   * @param endpoint Destination endpoint.
   * @param total_payload_length Payload length in bytes.
   * @return Receiver context, or NULL to drop the message (e.g. not enough space).
   */
  Receiver *(*prepare)(CommSession *session, const PebbleProtocolEndpoint *endpoint,
                       size_t total_payload_length);

  /**
   * @brief Write payload data of the current message.
   *
   * @param receiver Receiver context.
   * @param data Payload data.
   * @param length Length of @p data in bytes.
   */
  void (*write)(Receiver *receiver, const uint8_t *data, size_t length);

  /**
   * @brief Signal that the whole payload has been written.
   *
   * Schedules the endpoint handler and releases the Receiver context.
   *
   * @param receiver Receiver context.
   */
  void (*finish)(Receiver *receiver);

  /**
   * @brief Discard the current message because the session closed.
   *
   * Releases the Receiver context without calling the endpoint handler.
   *
   * @param receiver Receiver context.
   */
  void (*cleanup)(Receiver *receiver);
} ReceiverImplementation;

/**
 * @brief Pebble Protocol header parsing state of a session.
 *
 * Payloads are handed to the endpoint's @ref ReceiverImplementation.
 */
typedef struct ReceiveRouter {
  /** Bytes received for the current message so far, including the header. */
  uint16_t bytes_received;

  /** Number of upcoming inbound bytes to ignore. */
  uint16_t bytes_to_ignore;

  /** Payload length of the current message in bytes. */
  uint16_t msg_payload_length;

  /** Partially received header bytes. */
  uint8_t header_buffer[sizeof(PebbleProtocolHeader)];

  /** Receiver implementation of the current message. */
  const ReceiverImplementation *receiver_imp;
  /** Receiver context of the current message. */
  Receiver *receiver;
} ReceiveRouter;

/** @} */
