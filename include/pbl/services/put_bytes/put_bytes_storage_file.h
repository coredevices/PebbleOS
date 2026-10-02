/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "put_bytes_storage.h"

/**
 * @defgroup services_put_bytes_put_bytes_storage_file File storage
 * @ingroup services_put_bytes_put_bytes_storage
 * @brief Backend writing objects to PFS files.
 * @{
 */

/**
 * @brief Initialize storage, see pb_storage_init().
 *
 * Replaces any existing file with the name in @p info. A zero-sized object creates no file.
 *
 * @param[out] storage Storage.
 * @param object_type Object type.
 * @param total_size Object size in bytes.
 * @param info Object information.
 * @param append_offset Offset to resume at, 0 to start over.
 * @return true on success.
 */
bool pb_storage_file_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                          uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset);

/**
 * @brief Get the largest object size supported.
 *
 * Free PFS space.
 *
 * @param object_type Object type.
 * @return Maximum size in bytes.
 */
uint32_t pb_storage_file_get_max_size(PutBytesObjectType object_type);

/**
 * @brief Write data, see pb_storage_write().
 *
 * Only supports writing at @ref PutBytesStorage::current_offset.
 *
 * @param storage Storage.
 * @param offset Offset within the storage.
 * @param buffer Data.
 * @param length Length of @p buffer in bytes.
 */
void pb_storage_file_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                           uint32_t length);

/**
 * @brief Compute the CRC of the data written, see pb_storage_calculate_crc().
 *
 * Only @ref PutBytesCrcType_Legacy is supported.
 *
 * @param storage Storage.
 * @param crc_type CRC algorithm.
 * @return CRC.
 */
uint32_t pb_storage_file_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type);

/**
 * @brief Release storage, see pb_storage_deinit().
 *
 * Closes the file, deleting it on failure.
 *
 * @param storage Storage.
 * @param is_success Whether the transfer succeeded.
 */
void pb_storage_file_deinit(PutBytesStorage *storage, bool is_success);

/** @} */
