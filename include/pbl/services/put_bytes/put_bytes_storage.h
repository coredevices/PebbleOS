/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/put_bytes/put_bytes.h>

/**
 * @defgroup services_put_bytes_put_bytes_storage Put bytes storage
 * @ingroup services_put_bytes
 * @brief Backends storing received objects.
 *
 * Firmware, recovery and system resources use the raw flash backend, other objects the PFS file
 * backend (not built in recovery firmware).
 * @{
 */

struct PutBytesStorageImplementation;
/** @brief Backend operations, see @ref services_put_bytes_put_bytes_storage_internal. */
typedef struct PutBytesStorageImplementation PutBytesStorageImplementation;

/** @brief Storage of an object being received. */
typedef struct {
  /** Backend operations, NULL when not initialized. */
  const PutBytesStorageImplementation *impl;

  /** Backend private data. */
  void *impl_data;

  /**
   * Offset of the next append, advanced by pb_storage_append(). Set by pb_storage_init(), e.g.
   * past a metadata header or to the append offset.
   */
  uint32_t current_offset;
} PutBytesStorage;

/** @brief Object information passed to pb_storage_init(). */
typedef struct {
  /** Bank index of raw objects. */
  int index;
  /** NUL-terminated file name of file objects. */
  char filename[];
} PutBytesStorageInfo;

/**
 * @brief Write data at an offset, without moving @ref PutBytesStorage::current_offset.
 *
 * The file backend only supports writing at the current offset.
 *
 * @param storage Storage.
 * @param offset Offset within the storage.
 * @param buffer Data.
 * @param length Length of @p buffer in bytes.
 */
void pb_storage_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                      uint32_t length);

/**
 * @brief Append data at @ref PutBytesStorage::current_offset and advance it.
 *
 * @param storage Storage.
 * @param buffer Data.
 * @param length Length of @p buffer in bytes.
 */
void pb_storage_append(PutBytesStorage *storage, const uint8_t *buffer, uint32_t length);

/** @brief CRC algorithms. */
typedef enum {
  /** Legacy checksum, see pbl_crc32_legacy(). */
  PutBytesCrcType_Legacy = 0,
  /** CRC-32, see pbl_crc32(). Raw backend only. */
  PutBytesCrcType_CRC32,
} PutBytesCrcType;

/**
 * @brief Compute the CRC of the data written so far.
 *
 * Covers the data up to @ref PutBytesStorage::current_offset, excluding any metadata header.
 *
 * @param storage Storage.
 * @param crc_type CRC algorithm.
 * @return CRC.
 */
uint32_t pb_storage_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type);

/**
 * @brief Initialize storage for a new transfer.
 *
 * Selects the backend for @p object_type and rejects objects larger than it can hold.
 *
 * @param[out] storage Storage, zeroed beforehand.
 * @param object_type Object type.
 * @param total_size Object size in bytes.
 * @param info Object information.
 * @param append_offset If non-zero, resume a previously interrupted transfer at this offset
 *                      instead of starting over.
 * @return true on success.
 */
bool pb_storage_init(PutBytesStorage *storage, PutBytesObjectType object_type, uint32_t total_size,
                     PutBytesStorageInfo *info, uint32_t append_offset);

/**
 * @brief Release storage after a transfer.
 *
 * Does nothing if the storage is not initialized. A failed file transfer deletes the file.
 *
 * @param storage Storage.
 * @param is_success Whether the transfer succeeded.
 */
void pb_storage_deinit(PutBytesStorage *storage, bool is_success);

/**
 * @brief Recover the progress of a partially written object.
 *
 * Only supported for firmware, recovery and system resources.
 *
 * @param obj_type Object type.
 * @param[out] status Bytes written and their CRC.
 * @return true if @p status was filled in.
 */
bool pb_storage_get_status(PutBytesObjectType obj_type, PbInstallStatus *status);

/** @} */
