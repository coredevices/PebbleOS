/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "get_bytes_storage.h"

/**
 * @defgroup services_get_bytes_get_bytes_storage_flash Flash storage
 * @ingroup services_get_bytes_get_bytes_storage
 * @brief Backend reading a raw flash region.
 * @{
 */

/**
 * @brief Set up the storage, see gb_storage_setup().
 *
 * Fails if the region is empty or exceeds the flash size.
 *
 * @param[out] storage Storage.
 * @param object_type Object type.
 * @param info Request parameters.
 * @return true on success.
 */
bool gb_storage_flash_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                            GetBytesStorageInfo *info);

/**
 * @brief Get the object size, see gb_storage_get_size().
 *
 * @param storage Storage.
 * @param[out] size Object size in bytes.
 * @return @ref GET_BYTES_OK.
 */
GetBytesInfoErrorCode gb_storage_flash_get_size(GetBytesStorage *storage, uint32_t *size);

/**
 * @brief Read the next chunk, see gb_storage_read_next_chunk().
 *
 * @param storage Storage.
 * @param[out] buffer Destination.
 * @param len Number of bytes to read.
 * @return true on success.
 */
bool gb_storage_flash_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len);

/**
 * @brief Release the storage, see gb_storage_cleanup().
 *
 * Frees the backend state.
 *
 * @param storage Storage.
 * @param successful The whole object was sent.
 */
void gb_storage_flash_cleanup(GetBytesStorage *storage, bool successful);

/** @} */
