/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/protocol.h"

#include <stddef.h>
#include <stdint.h>

typedef struct CommSession CommSession;

//! Pebble Protocol endpoint handler.
//! @see protocol_endpoints_table.json
typedef void (*PebbleProtocolEndpointHandler)(CommSession *session, const uint8_t *data,
                                              size_t length);

typedef enum {
  PebbleProtocolAccessPublic  = 1 << 0,  // reserved for 3rd party phone apps
  PebbleProtocolAccessPrivate = 1 << 1,  // reserved for Pebble phone app
  PebbleProtocolAccessAny = ~0,          // anyone is allowed
  PebbleProtocolAccessNone = 0,
} PebbleProtocolAccess;

typedef struct ReceiverImplementation ReceiverImplementation;

//! The info associated with a single Pebble Protocol endpoint.
//! @see protocol_endpoints_table.json
typedef struct PebbleProtocolEndpoint {
  uint16_t endpoint_id;
  PebbleProtocolEndpointHandler handler;
  PebbleProtocolAccess access_mask;
  const ReceiverImplementation *receiver_imp;
  const void *receiver_opt;
} PebbleProtocolEndpoint;

//! Opaque type, can be anything, up to ReceiverImplementation what it actually contains.
//! Receiver is the context associated with a messages that is currently being received and only
//! that one message. At any time a message is being received by a CommSession, there is a
//! one-to-one relationship between that CommSession and the Receiver, because messages cannot be
//! interleaved inside one Pebble Protocol data stream.
//! @see ReceiverImplementation
typedef struct Receiver Receiver;


//! A ReceiverImplementation is responsible for creating a Receiver context (see "prepare"),
//! buffering inbound message payload data (see "write") and finally scheduling the execution of
//! the endpoint handler (see "finish").
//! A ReceiverImplementation can be specific to the endpoint, for example, Put Bytes has a special
//! receiver implementation, because of the big buffer it requires.
//! However, a ReceiverImplementation can also be shared amongst multiple endpoints, which makes
//! sense if the buffering needs for a set of endpoints are equal or very similar.
//! @note There can be multiple CommSessions writing (partial) messages concurrently.
//! The receiver is responsible for dealing with this. So if the messages are collected in one big
//! circular buffer, it will have to take special measures to allow another CommSession to start
//! writing something while another CommSession's message has not been fully received yet.
//! @note All functions must be implemented, none of the function pointers can point to NULL.
typedef struct ReceiverImplementation {
  //! Prepares a Receiver context.
  //! If there is not enough space left to be able to buffer the complete payload, NULL can be
  //! returned to drop/ignore the message.
  //! @param receiver_opt Optional per-endpoint configuration for the receiver, assigned through
  //! protocol_endpoints_table.json.
  Receiver * (*prepare)(CommSession *session, const PebbleProtocolEndpoint *endpoint,
                        size_t total_payload_length);

  //! Writes payload data of the current message to the Receiver context.
  void (*write)(Receiver *receiver, const uint8_t *data, size_t length);

  //! Indicates the complete payload data of the current message has been written.
  //! When "finish" is called, execution of the endpoint handler should be scheduled by the
  //! implementation. The implementation should also take care of cleaning up the Receiver context.
  void (*finish)(Receiver *receiver);

  //! Called when the session is closed, to clean up the Receiver context.
  //! The message will be discarded and not be delivered to the endpoint handler.
  void (*cleanup)(Receiver *receiver);
} ReceiverImplementation;


//! ReceiveRouter contains the state associated with parsing the Pebble Protocol header.
//! This module will call the ReceiverImplementation to buffer and process the message payload.
typedef struct ReceiveRouter {
  //! Total number of bytes received for the current message so far, including the header.
  uint16_t bytes_received;

  //! Number of inbound bytes that should be ignored after the current point.
  uint16_t bytes_to_ignore;

  //! Expected payload length of the current message in bytes.
  uint16_t msg_payload_length;

  //! In case the number of bytes received was less than the length of the header,
  //! this buffer will be used to store those few bytes that were received.
  uint8_t header_buffer[sizeof(PebbleProtocolHeader)];

  //! Receiver of the current message.
  const ReceiverImplementation *receiver_imp;
  Receiver *receiver;
} ReceiveRouter;
