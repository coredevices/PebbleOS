/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/status_codes.h"

/**
 * @defgroup services_blob_db_prefs_db Preferences database
 * @ingroup services_blob_db
 * @brief System preferences exposed to the phone (::BlobDBIdPrefs).
 * @{
 */

/** @brief Initialize the preferences database. */
void prefs_db_init(void);

/**
 * @brief Insert or replace a record in the preferences database.
 *
 * Writes the backing store of the system preferences.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the preferences database.
 *
 * Returns @c E_INVALID_ARGUMENT for unknown keys.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int prefs_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the preferences database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the preferences database.
 *
 * Not supported: always returns @c E_INVALID_OPERATION.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t prefs_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the preferences database.
 *
 * Not supported: always returns @c E_INVALID_OPERATION.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t prefs_db_flush(void);

/** @} */
