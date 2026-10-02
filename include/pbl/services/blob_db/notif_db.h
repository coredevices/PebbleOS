/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/status_codes.h"
#include "pbl/services/timeline/item.h"

/**
 * @defgroup services_blob_db_notif_db Notification database
 * @ingroup services_blob_db
 * @brief BlobDB front end of notification storage (::BlobDBIdNotifs).
 * @{
 */

/** @brief Initialize the notification database. */
void notif_db_init(void);

/**
 * @brief Insert or replace a record in the notification database.
 *
 * The key is the notification UUID and the value a serialized timeline item. A new
 * notification is stored and announced; for an existing one only the status bits are updated.
 * New notifications that already carry status bits are ignored.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t notif_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the notification database.
 *
 * The key is the notification UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int notif_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the notification database.
 *
 * Not implemented: returns @c S_SUCCESS without writing to @p val_out.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t notif_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the notification database.
 *
 * The key is the notification UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t notif_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the notification database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t notif_db_flush(void);

/** @} */
