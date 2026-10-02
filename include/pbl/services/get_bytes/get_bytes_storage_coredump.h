/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "get_bytes_storage.h"

/**
 * @defgroup services_get_bytes_get_bytes_storage_coredump Core dump storage
 * @ingroup services_get_bytes_get_bytes_storage
 * @brief Backend reading the most recent core dump from flash.
 * @{
 */

/**
 * @brief Set up the storage, see gb_storage_setup().
 *
 * Only allocates the backend state.
 *
 * @param[out] storage Storage.
 * @param object_type Object type.
 * @param info Request parameters.
 * @return true on success.
 */
bool gb_storage_coredump_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                               GetBytesStorageInfo *info);

/**
 * @brief Get the object size, see gb_storage_get_size().
 *
 * @param storage Storage.
 * @param[out] size Object size in bytes.
 * @retval GET_BYTES_OK Success.
 * @retval GET_BYTES_DOESNT_EXIST No (unread) core dump.
 * @retval GET_BYTES_CORRUPTED The core dump is corrupted.
 */
GetBytesInfoErrorCode gb_storage_coredump_get_size(GetBytesStorage *storage, uint32_t *size);

/**
 * @brief Read the next chunk, see gb_storage_read_next_chunk().
 *
 * @param storage Storage.
 * @param[out] buffer Destination.
 * @param len Number of bytes to read.
 * @return true on success.
 */
bool gb_storage_coredump_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len);

/**
 * @brief Release the storage, see gb_storage_cleanup().
 *
 * Marks the core dump as read if @p successful, then frees the backend state.
 *
 * @param storage Storage.
 * @param successful The whole object was sent.
 */
void gb_storage_coredump_cleanup(GetBytesStorage *storage, bool successful);

/** @} */
