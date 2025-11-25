/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"

#include <stdbool.h>
#include <stdint.h>

#include "util/attributes.h"

typedef uint16_t BlobDBToken;

//! Response / result values
typedef enum PACKED {
  BLOB_DB_SUCCESS             = 0x01,
  BLOB_DB_GENERAL_FAILURE     = 0x02,
  BLOB_DB_INVALID_OPERATION   = 0x03,
  BLOB_DB_INVALID_DATABASE_ID = 0x04,
  BLOB_DB_INVALID_DATA        = 0x05,
  BLOB_DB_KEY_DOES_NOT_EXIST  = 0x06,
  BLOB_DB_DATABASE_FULL       = 0x07,
  BLOB_DB_DATA_STALE          = 0x08,
  BLOB_DB_DB_NOT_SUPPORTED    = 0x09,
  BLOB_DB_DB_LOCKED           = 0x0A,
  BLOB_DB_TRY_LATER           = 0x0B,
} BlobDBResponse;
_Static_assert(sizeof(BlobDBResponse) == 1, "BlobDBResponse is larger than 1 byte");

#define RESPONSE_MASK (1 << 7)

typedef enum PACKED {
  BLOB_DB_COMMAND_INSERT = 0x01,
  BLOB_DB_COMMAND_READ   = 0x02,  // Not implemented yet
  BLOB_DB_COMMAND_UPDATE = 0x03,  // Not implemented yet
  BLOB_DB_COMMAND_DELETE = 0x04,
  BLOB_DB_COMMAND_CLEAR  = 0x05,

  // Commands below were added as part of sync and may not all be supported by the phone
  BLOB_DB_COMMAND_DIRTY_DBS  = 0x06,
  BLOB_DB_COMMAND_START_SYNC = 0x07,
  BLOB_DB_COMMAND_WRITE      = 0x08,
  BLOB_DB_COMMAND_WRITEBACK  = 0x09,
  BLOB_DB_COMMAND_SYNC_DONE  = 0x0A,
  // Response commands
  BLOB_DB_COMMAND_DIRTY_DBS_RESPONSE  = BLOB_DB_COMMAND_DIRTY_DBS  | RESPONSE_MASK,
  BLOB_DB_COMMAND_START_SYNC_RESPONSE = BLOB_DB_COMMAND_START_SYNC | RESPONSE_MASK,
  BLOB_DB_COMMAND_WRITE_RESPONSE      = BLOB_DB_COMMAND_WRITE      | RESPONSE_MASK,
  BLOB_DB_COMMAND_WRITEBACK_RESPONSE  = BLOB_DB_COMMAND_WRITEBACK  | RESPONSE_MASK,
  BLOB_DB_COMMAND_SYNC_DONE_RESPONSE  = BLOB_DB_COMMAND_SYNC_DONE  | RESPONSE_MASK,
} BlobDBCommand;
_Static_assert(sizeof(BlobDBCommand) == 1, "BlobDBCommand is larger than 1 byte");


const uint8_t *endpoint_private_read_token_db_id(const uint8_t *iter, BlobDBToken *out_token,
                                                 BlobDBId *out_db_id);

void blob_db_enabled(bool enabled);
