/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "get_bytes.h"

#include <stdint.h>

#include "kernel/core_dump_private.h"
#include "pbl/services/comm_session/session.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_get_bytes_get_bytes_private Get bytes protocol
 * @ingroup services_get_bytes
 * @brief Get bytes endpoint messages.
 * @{
 */

/** @brief Get bytes endpoint ID. */
static const uint16_t GET_BYTES_ENDPOINT_ID = 9000;

/** @brief Header of every request and response. */
typedef struct PBL_PACKED {
  /** Command, a @ref GetBytesCmd value. */
  uint8_t cmd_id;
  /** Transaction ID chosen by the phone, echoed in responses. */
  uint8_t transaction_id;
} GetBytesHeader;

/** @brief @ref GET_BYTES_CMD_GET_FILE request. */
typedef struct PBL_PACKED {
  /** Header. */
  GetBytesHeader hdr;
  /** Length of @ref filename, excluding the terminating NUL. */
  uint8_t filename_len;
  /** NUL-terminated file name. */
  char filename[];
} GetBytesFileHeader;

/** @brief @ref GET_BYTES_CMD_GET_FLASH request. */
typedef struct PBL_PACKED {
  /** Header. */
  GetBytesHeader hdr;
  /** Flash address. */
  uint32_t start_addr;
  /** Length in bytes. */
  uint32_t len;
} GetBytesFlashHeader;

/** @brief Get bytes commands. */
typedef enum {
  /** Request the most recent core dump; the request is a bare @ref GetBytesHeader. */
  GET_BYTES_CMD_GET_COREDUMP = 0,
  /** Response with the object size or an error, see @ref GetBytesRspObjectInfo. */
  GET_BYTES_CMD_OBJECT_INFO = 1,
  /** Response with object data, see @ref GetBytesRspObjectData. */
  GET_BYTES_CMD_OBJECT_DATA = 2,
  /** Request a file, see @ref GetBytesFileHeader. */
  GET_BYTES_CMD_GET_FILE = 3,
  /** Request a flash region, see @ref GetBytesFlashHeader. */
  GET_BYTES_CMD_GET_FLASH = 4,
  /** Request the most recent core dump only if it has not been read yet. */
  GET_BYTES_CMD_GET_NEW_COREDUMP = 5,
} GetBytesCmd;

/** @brief @ref GET_BYTES_CMD_OBJECT_INFO response. */
typedef struct PBL_PACKED {
  /** Header. */
  GetBytesHeader hdr;
  /** A @ref GetBytesInfoErrorCode; data responses follow only for @ref GET_BYTES_OK. */
  uint8_t error_code;
  /** Total object size in bytes, 0 on error. */
  uint32_t num_bytes;
} GetBytesRspObjectInfo;

/** @brief @ref GET_BYTES_CMD_OBJECT_DATA response. */
typedef struct PBL_PACKED {
  /** Header. */
  GetBytesHeader hdr;
  /** Offset of @ref data in the object. */
  uint32_t byte_offset;
  /** Object data. */
  uint8_t data[];
} GetBytesRspObjectData;

/** @brief Error response sent asynchronously. */
typedef struct GetBytesErrorResponse {
  /** Session to reply on. */
  CommSession *session;
  /** Transaction ID of the request. */
  int8_t transaction_id;
  /** Error code. */
  GetBytesInfoErrorCode result;
} GetBytesErrorResponse;

/** @} */
