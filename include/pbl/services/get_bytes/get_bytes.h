/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_get_bytes Get bytes
 * @ingroup services
 * @brief Transfer of core dumps, files and raw flash from the watch to the phone.
 *
 * A Pebble Protocol endpoint. The phone sends a request (see @ref GetBytesCmd), and the watch
 * answers with a @c GET_BYTES_CMD_OBJECT_INFO message holding an error code and the object size,
 * followed on success by @c GET_BYTES_CMD_OBJECT_DATA chunks until the whole object is sent.
 * Multi-byte fields are big-endian. Only one transfer runs at a time. File and flash reads are
 * not available in release builds.
 *
 * Objects are read through a @ref services_get_bytes_get_bytes_storage backend.
 * @{
 */

/** @brief Object types transferred over get bytes. */
typedef enum {
  /** Unknown. */
  GetBytesObjectUnknown = 0x00,
  /** Core dump. */
  GetBytesObjectCoredump = 0x01,
  /** PFS file. */
  GetBytesObjectFile = 0x02,
  /** Raw flash region. */
  GetBytesObjectFlash = 0x03
} GetBytesObjectType;

/** @brief Error codes of the @c GET_BYTES_CMD_OBJECT_INFO response. */
typedef enum {
  /** Success, data follows. */
  GET_BYTES_OK = 0,
  /** Invalid or unsupported request. */
  GET_BYTES_MALFORMED_COMMAND = 1,
  /** Another transfer is in progress. */
  GET_BYTES_ALREADY_IN_PROGRESS = 2,
  /** The object does not exist, e.g. no (new) core dump. */
  GET_BYTES_DOESNT_EXIST = 3,
  /** The object is corrupted. */
  GET_BYTES_CORRUPTED = 4,
} GetBytesInfoErrorCode;

/** @} */
