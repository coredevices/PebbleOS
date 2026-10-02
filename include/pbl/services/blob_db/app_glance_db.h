/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/app_glances/app_glance_service.h"
#include "system/status_codes.h"
#include <time.h>
#include "pbl/util/uuid.h"

#include <stdint.h>

/**
 * @defgroup services_blob_db_app_glance_db App glance database
 * @ingroup services_blob_db
 * @brief App glances (::BlobDBIdAppGlance), keyed by app UUID.
 * @{
 */

/**
 * @brief Serialize and store the glance of an app.
 *
 * @param uuid App UUID.
 * @param glance Glance to store.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_insert_glance(const Uuid *uuid, const AppGlance *glance);

/**
 * @brief Read and deserialize the glance of an app.
 *
 * @param uuid App UUID.
 * @param[out] glance_out Glance.
 * @retval S_SUCCESS Read.
 * @retval E_DOES_NOT_EXIST No glance stored for the app.
 * @return Other error codes on failure.
 */
status_t app_glance_db_read_glance(const Uuid *uuid, AppGlance *glance_out);

/**
 * @brief Read the creation time of the glance of an app.
 *
 * @param uuid App UUID.
 * @param[out] time_out Creation time.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_read_creation_time(const Uuid *uuid, time_t *time_out);

/**
 * @brief Delete the glance of an app.
 *
 * @param uuid App UUID.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_delete_glance(const Uuid *uuid);

/** @brief Initialize the app glance database. */
void app_glance_db_init(void);

/**
 * @brief Delete all records of the app glance database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_flush(void);

/**
 * @brief Compact the settings file backing the app glance database.
 *
 * Shrinks a grown file back toward its initial allocation.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_compact(void);

/**
 * @brief Insert or replace a record in the app glance database.
 *
 * The key is the app UUID and the value a serialized glance (::SerializedAppGlanceHeader).
 * The glance must have the current version, be newer than the stored one, and belong to an
 * installed or system app; excess slices are trimmed. Triggers a fetch of uncached apps.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the app glance database.
 *
 * The key is the app UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int app_glance_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the app glance database.
 *
 * The key is the app UUID. Records with an outdated version are deleted and reported as
 * @c E_DOES_NOT_EXIST.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the app glance database.
 *
 * The key is the app UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_glance_db_delete(const uint8_t *key, int key_len);

/** @} */
