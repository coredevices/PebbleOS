/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup services_comm_session_protocol Pebble Protocol framing
 * @ingroup services_comm_session
 * @brief Pebble Protocol message header and size limits.
 * @{
 */

/** @brief Header preceding every Pebble Protocol message. */
typedef struct PBL_PACKED {
  /** Payload length in bytes, big endian. */
  uint16_t length;
  /** Destination endpoint ID, big endian. */
  uint16_t endpoint_id;
} PebbleProtocolHeader;

/** @brief Maximum inbound payload size in bytes for private (Pebble app) endpoints. */
#define COMM_PRIVATE_MAX_INBOUND_PAYLOAD_SIZE 2044
/** @brief Maximum inbound payload size in bytes for public (third party) endpoints. */
#define COMM_PUBLIC_MAX_INBOUND_PAYLOAD_SIZE 144
// TODO: If we have memory to spare, let's crank this up to improve data spooling
/** @brief Maximum outbound payload size in bytes. */
#define COMM_MAX_OUTBOUND_PAYLOAD_SIZE 656

/** @} */
