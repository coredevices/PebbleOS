/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/comm_session/session.h"
#include "pbl/kernel/compiler.h"

#include <stdint.h>

/**
 * @defgroup services_comm_session_meta_endpoint Meta endpoint
 * @ingroup services_comm_session
 * @brief Protocol error responses sent on the meta endpoint (ID 0).
 * @{
 */

/** @brief Meta endpoint error code. */
typedef enum {
  /** No error. */
  MetaResponseCodeNoError = 0x0,
  /** Message could not be parsed; the response carries no endpoint ID. */
  MetaResponseCodeCorruptedMessage = 0xd0,
  /** Sender is not allowed to use the endpoint. */
  MetaResponseCodeDisallowed = 0xdd,
  /** No handler for the endpoint. */
  MetaResponseCodeUnhandled = 0xdc,
} MetaResponseCode;

/** @brief Meta endpoint response to send. */
typedef struct MetaResponseInfo {
  /** Session to send the response to. */
  CommSession *session;
  /** Response payload. */
  struct PBL_PACKED {
    /** Error code, a @ref MetaResponseCode. */
    uint8_t error_code;
    /** Endpoint ID the error refers to. */
    uint16_t endpoint_id;
  } payload;
} MetaResponseInfo;

/**
 * @brief Send a meta endpoint response asynchronously from KernelBG.
 *
 * The response is copied, so @p meta_response_info need not outlive the call. Set
 * @c payload.endpoint_id in CPU byte order; it is converted to big endian before sending.
 *
 * @param meta_response_info Response to send.
 */
void meta_endpoint_send_response_async(const MetaResponseInfo *meta_response_info);

/** @} */
