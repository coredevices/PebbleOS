/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "protobuf_log.h"

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <pb_decode.h>
#include <pb_encode.h>

/**
 * @defgroup services_protobuf_log_protobuf_log_private Protobuf log internals
 * @ingroup services_protobuf_log
 * @brief Record header and session structure.
 * @{
 */

/** @brief Header placed before the encoded payload in each record. */
typedef struct PBL_PACKED {
  /** Size of the encoded payload in bytes. */
  uint16_t msg_size;
} PLogMessageHdr;

/** @brief Protobuf log session, behind a ProtobufLogRef. */
typedef struct PLogSession {
  // TODO Change comments from MeasurementSet to MeasurementSet/Events

  /** Configuration, with the measurement types copied after the structure. */
  ProtobufLogConfig config;
  /** Record buffer: @ref PLogMessageHdr followed by the encoded payload. */
  uint8_t *msg_buffer;
  /**
   * Encoded data accumulated so far (a measurement set or events), wrapped into a payload in
   * @ref msg_buffer on flush.
   */
  uint8_t *data_buffer;
  /** Record size in bytes. */
  size_t max_msg_size;
  /** Maximum size of the accumulated data. */
  size_t max_data_size;
  /** Stream writing to @ref data_buffer. */
  pb_ostream_t data_stream;
  /** UTC time the current data started. */
  time_t start_utc;
  /** Transport. */
  ProtobufLogTransportCB transport;
} PLogSession;

/** @} */
