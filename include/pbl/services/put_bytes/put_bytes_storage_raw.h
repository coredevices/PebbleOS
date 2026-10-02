/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "put_bytes_storage.h"

#include <stdbool.h>

/**
 * @defgroup services_put_bytes_put_bytes_storage_raw Raw flash storage
 * @ingroup services_put_bytes_put_bytes_storage
 * @brief Backend writing firmware, recovery and system resources to flash.
 * @{
 */

/**
 * @brief Initialize storage, see pb_storage_init().
 *
 * Erases the destination region, unless resuming at @p append_offset. Firmware images start
 * after a @c FirmwareDescription header written on commit.
 *
 * @param[out] storage Storage.
 * @param object_type Object type.
 * @param total_size Object size in bytes.
 * @param info Object information.
 * @param append_offset Offset to resume at, 0 to start over.
 * @return true on success.
 */
bool pb_storage_raw_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                         uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset);

/**
 * @brief Get the largest object size supported.
 *
 * Size of the destination region.
 *
 * @param object_type Object type.
 * @return Maximum size in bytes.
 */
uint32_t pb_storage_raw_get_max_size(PutBytesObjectType object_type);

/**
 * @brief Write data, see pb_storage_write().
 *
 * @p offset is relative to the start of the region.
 *
 * @param storage Storage.
 * @param offset Offset within the storage.
 * @param buffer Data.
 * @param length Length of @p buffer in bytes.
 */
void pb_storage_raw_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                          uint32_t length);

/**
 * @brief Compute the CRC of the data written, see pb_storage_calculate_crc().
 *
 * Covers the data written, excluding the firmware description header.
 *
 * @param storage Storage.
 * @param crc_type CRC algorithm.
 * @return CRC.
 */
uint32_t pb_storage_raw_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type);

/**
 * @brief Release storage, see pb_storage_deinit().
 *
 * Restores the Bluetooth responsiveness lowered during the erase.
 *
 * @param storage Storage.
 * @param is_success Whether the transfer succeeded.
 */
void pb_storage_raw_deinit(PutBytesStorage *storage, bool is_success);

/**
 * @brief Recover the progress of a partially written object, see pb_storage_get_status().
 *
 * Scans the region backwards for the last programmed byte. One byte less than written is
 * reported, so a firmware update always sends the last chunk again.
 *
 * @param obj_type Object type.
 * @param[out] status Bytes written and their legacy CRC.
 * @return true if @p status was filled in.
 */
bool pb_storage_raw_get_status(PutBytesObjectType obj_type, PbInstallStatus *status);

/** @} */
