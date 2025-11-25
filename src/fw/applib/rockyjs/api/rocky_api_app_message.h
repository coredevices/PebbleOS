/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "rocky_api.h"

#include <util/attributes.h>

#define POSTMESSAGE_PROTOCOL_MIN_VERSION ((uint8_t)1)
#define POSTMESSAGE_PROTOCOL_MAX_VERSION ((uint8_t)1)

//! Max size in bytes of the largest Chunk payload that can be sent out.
#define POSTMESSAGE_PROTOCOL_MAX_TX_CHUNK_SIZE ((uint16_t)1000)

//! Max size in bytes of the largest Chunk payload that can be received.
#define POSTMESSAGE_PROTOCOL_MAX_RX_CHUNK_SIZE ((uint16_t)1000)

//! Message Types, expected values are byte arrays. See structs below.
typedef enum {
  PostMessageKeyInvalid = 0,

  //! Has no payload.
  PostMessageKeyResetRequest = 1,

  //! Has PostMessageResetCompletePayload as payload.
  PostMessageKeyResetComplete = 2,

  //! Has PostMessageChunkPayload as payload.
  PostMessageKeyChunk = 3,

  //! Has PostMessageUnsupportedErrorPayload as payload.
  PostMessageKeyUnsupportedError = 4,

  PostMessageKey_Count
} PostMessageKey;

typedef struct PACKED {
  //! Lowest supported version (inclusive)
  uint8_t min_supported_version;

  //! Highest supported version (inclusive)
  uint8_t max_supported_version;

  //! Maximum Chunk size (little-endian) that the sender of ResetComplete is capable of sending.
  uint16_t max_tx_chunk_size;

  //! Maximum Chunk size (little-endian) that the sender of ResetComplete is capable of receiving.
  uint16_t max_rx_chunk_size;
} PostMessageResetCompletePayload;

_Static_assert(sizeof(PostMessageResetCompletePayload) >= 6,
               "Should never be smaller than the V1 payload!");

typedef struct PACKED {
  union PACKED {
    //! Header for first Chunk in a sequence of chunks. Valid when is_first == true.
    struct PACKED {
      //! Total size of the object (sum of the lengths of all chunk_data in the Chunk sequence)
      //! including a zero byte at the end of the JSON string.
      uint32_t total_size_bytes:31;
      //! Always set to true for the first Chunk.
      bool is_first:1;
    };
    //! Header for continuation Chunks in a sequence of chunks.  Valid when is_first == false.
    struct PACKED {
      //! The offset of the chunk_data into the fully assembled object.
      uint32_t offset_bytes:31;
      //! Always set to false for a continuation Chunk.
      //! @note the compiler doesn't like it if the field is called the same, even though it's of
      //! the same type and occupies the same bits...
      bool continuation_is_first:1;
    };
  };
  //! JSON string data (potentially a partial fragment).
  //! The final Chunk's chunk_data MUST be zero-terminated!
  char chunk_data[0];
} PostMessageChunkPayload;

typedef enum {
  PostMessageErrorUnsupportedVersion,
  PostMessageErrorMalformedResetComplete,
} PostMessageError;

typedef struct PACKED {
  PostMessageError error_code:8;
} PostMessageUnsupportedErrorPayload;

//! See statechart diagram over here:
//! https://pebbletechnology.atlassian.net/wiki/display/PRODUCT/postMessage%28%29+protocol
typedef enum {
  //! Transport (AppMessage / PP) Disconnected
  PostMessageStateDisconnected = 0,

  //! No negotiated state, wait for remote to request reset
  PostMessageStateAwaitingResetRequest,

  //! Waiting for a reset complete message, the remote side had initiated the ResetRequest.
  PostMessageStateAwaitingResetCompleteRemoteInitiated,

  //! Waiting for a reset complete message, the local side had initiated the ResetRequest.
  PostMessageStateAwaitingResetCompleteLocalInitiated,

  //! Transport connected, negotiation complete and ready to send and receive payload chunks.
  PostMessageStateSessionOpen,

  PostMessageState_Count
} PostMessageState;

extern const RockyGlobalAPI APP_MESSAGE_APIS;
