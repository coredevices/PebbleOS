/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"

#include <stdbool.h>
#include <stdint.h>

#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_blob_db_endpoint_private Protocol
 * @ingroup services_blob_db
 * @brief BlobDB Pebble Protocol definitions.
 *
 * The phone writes on endpoint @c 0xb1db and sync runs on @c 0xb2db; see
 * @c docs/reference/blob-db.md for message layouts.
 * @{
 */

/** @brief Message token, echoed in responses to match them with requests. */
typedef uint16_t BlobDBToken;

/** @brief Response codes. */
typedef enum PBL_PACKED {
  /** Success. */
  BLOB_DB_SUCCESS = 0x01,
  /** Unspecified error. */
  BLOB_DB_GENERAL_FAILURE = 0x02,
  /** Unknown or unsupported command. */
  BLOB_DB_INVALID_OPERATION = 0x03,
  /** Invalid or disabled database (@c E_RANGE). */
  BLOB_DB_INVALID_DATABASE_ID = 0x04,
  /** Malformed message or rejected record (@c E_INVALID_ARGUMENT). */
  BLOB_DB_INVALID_DATA = 0x05,
  /** Key not found (@c E_DOES_NOT_EXIST). */
  BLOB_DB_KEY_DOES_NOT_EXIST = 0x06,
  /** Out of storage (@c E_OUT_OF_STORAGE). */
  BLOB_DB_DATABASE_FULL = 0x07,
  /** Watch copy is newer (@c E_INVALID_OPERATION). */
  BLOB_DB_DATA_STALE = 0x08,
  /** Database not supported. */
  BLOB_DB_DB_NOT_SUPPORTED = 0x09,
  /** Database locked. */
  BLOB_DB_DB_LOCKED = 0x0A,
  /** Endpoint not accepting messages yet. */
  BLOB_DB_TRY_LATER = 0x0B,
} BlobDBResponse;
_Static_assert(sizeof(BlobDBResponse) == 1, "BlobDBResponse is larger than 1 byte");

/** @brief Bit set in the command of a response. */
#define RESPONSE_MASK (1 << 7)

/** @brief Commands. */
typedef enum PBL_PACKED {
  /** Insert a record. */
  BLOB_DB_COMMAND_INSERT = 0x01,
  /** Read a record; not implemented. */
  BLOB_DB_COMMAND_READ = 0x02,
  /** Update a record; not implemented. */
  BLOB_DB_COMMAND_UPDATE = 0x03,
  /** Delete a record. */
  BLOB_DB_COMMAND_DELETE = 0x04,
  /** Delete all records of a database. */
  BLOB_DB_COMMAND_CLEAR = 0x05,

  /**
   * List databases with dirty records.
   *
   * This and the following commands belong to sync; not every phone supports them.
   */
  BLOB_DB_COMMAND_DIRTY_DBS = 0x06,
  /** Start syncing a database. */
  BLOB_DB_COMMAND_START_SYNC = 0x07,
  /** Watch write of a single record. */
  BLOB_DB_COMMAND_WRITE = 0x08,
  /** Watch write-back of a dirty record during a database sync. */
  BLOB_DB_COMMAND_WRITEBACK = 0x09,
  /** Database sync finished. */
  BLOB_DB_COMMAND_SYNC_DONE = 0x0A,
  /** Protocol version query. */
  BLOB_DB_COMMAND_VERSION = 0x0B,
  /** Mark all records dirty; only supported by the settings database. */
  BLOB_DB_COMMAND_DIRTY_ALL = 0x0C,
  /** Insert a record unless the watch copy is newer. */
  BLOB_DB_COMMAND_INSERT_WITH_TIMESTAMP = 0x0D,
  /** Response to ::BLOB_DB_COMMAND_DIRTY_DBS. */
  BLOB_DB_COMMAND_DIRTY_DBS_RESPONSE = BLOB_DB_COMMAND_DIRTY_DBS | RESPONSE_MASK,
  /** Response to ::BLOB_DB_COMMAND_START_SYNC. */
  BLOB_DB_COMMAND_START_SYNC_RESPONSE = BLOB_DB_COMMAND_START_SYNC | RESPONSE_MASK,
  /** Response to ::BLOB_DB_COMMAND_WRITE. */
  BLOB_DB_COMMAND_WRITE_RESPONSE = BLOB_DB_COMMAND_WRITE | RESPONSE_MASK,
  /** Response to ::BLOB_DB_COMMAND_WRITEBACK. */
  BLOB_DB_COMMAND_WRITEBACK_RESPONSE = BLOB_DB_COMMAND_WRITEBACK | RESPONSE_MASK,
  /** Response to ::BLOB_DB_COMMAND_SYNC_DONE. */
  BLOB_DB_COMMAND_SYNC_DONE_RESPONSE = BLOB_DB_COMMAND_SYNC_DONE | RESPONSE_MASK,
  /** Response to ::BLOB_DB_COMMAND_VERSION. */
  BLOB_DB_COMMAND_VERSION_RESPONSE = BLOB_DB_COMMAND_VERSION | RESPONSE_MASK,
} BlobDBCommand;
_Static_assert(sizeof(BlobDBCommand) == 1, "BlobDBCommand is larger than 1 byte");

/**
 * @brief Parse the token and database id at the start of a message body.
 *
 * @param iter Message body, after the command byte.
 * @param[out] out_token Token.
 * @param[out] out_db_id Database id.
 * @return Pointer past the parsed fields.
 */
const uint8_t *endpoint_private_read_token_db_id(const uint8_t *iter, BlobDBToken *out_token,
                                                 BlobDBId *out_db_id);

/**
 * @brief Enable or disable message handling on both BlobDB endpoints.
 *
 * While disabled, commands are answered with ::BLOB_DB_TRY_LATER.
 *
 * @param enabled true to accept messages.
 */
void blob_db_enabled(bool enabled);

/** @} */
