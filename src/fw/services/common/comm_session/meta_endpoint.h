/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session.h"
#include "util/attributes.h"

#include <stdint.h>

typedef enum {
  MetaResponseCodeNoError = 0x0,
  MetaResponseCodeCorruptedMessage = 0xd0,
  MetaResponseCodeDisallowed = 0xdd,
  MetaResponseCodeUnhandled = 0xdc,
} MetaResponseCode;

typedef struct MetaResponseInfo {
  CommSession *session;
  struct PACKED {
    //! @see MetaResponseCode
    uint8_t error_code;
    uint16_t endpoint_id;
  } payload;
} MetaResponseInfo;

//! Sends out a response for the "meta" endpoint, asynchronously on KernelBG.
//! @note The endpoint_id must be set in Little Endian byte order. This function will take care
//! of swapping it to the correct endianness.
void meta_endpoint_send_response_async(const MetaResponseInfo *meta_response_info);
