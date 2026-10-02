/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "pbl/util/uuid.h"
#include "process_management/app_install_manager.h"
#include "process_management/pebble_process_info.h"
#include "system/status_codes.h"
#include "pbl/kernel/compiler.h"

/**
 * @defgroup services_blob_db_app_db App database
 * @ingroup services_blob_db
 * @brief Installed apps (::BlobDBIdApps).
 *
 * Records are keyed by app UUID on the wire and stored keyed by @c AppInstallId.
 * @{
 */

/** @brief App database record, as sent by the phone. */
typedef struct PBL_PACKED {
  /** App UUID. */
  Uuid uuid;
  /** Process info flags (visibility, process type, worker), as in the app binary. */
  uint32_t info_flags;
  /** Resource id of the app icon. */
  uint32_t icon_resource_id;
  /** App version. */
  Version app_version;
  /** SDK version the app was built with. */
  Version sdk_version;
  /** Background color of the app face. */
  GColor8 app_face_bg_color;
  /** Template id, unused by the firmware. */
  uint8_t template_id;
  /** App name. */
  char name[APP_NAME_SIZE_BYTES];
} AppDBEntry;

/**
 * @brief Callback of app_db_enumerate_entries().
 *
 * @param install_id Install id of the app.
 * @param entry Record; only valid during the call.
 * @param data User data.
 */
typedef void (*AppDBEnumerateCb)(AppInstallId install_id, AppDBEntry *entry, void *data);

/**
 * @brief Find the install id of an app.
 *
 * @param uuid App UUID.
 * @return Install id, @c INSTALL_ID_INVALID if the app is not in the database, or a negative
 * error code if the database could not be opened.
 */
AppInstallId app_db_get_install_id_for_uuid(const Uuid *uuid);

/**
 * @brief Read the record of an app by UUID.
 *
 * @param uuid App UUID.
 * @param[out] entry Record.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_get_app_entry_for_uuid(const Uuid *uuid, AppDBEntry *entry);

/**
 * @brief Read the record of an app by install id.
 *
 * @param app_id Install id.
 * @param[out] entry Record.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_get_app_entry_for_install_id(AppInstallId app_id, AppDBEntry *entry);

/**
 * @brief Call a function for every app in the database.
 *
 * The database is locked during the iteration.
 *
 * @param cb Callback.
 * @param data User data passed to @p cb.
 */
void app_db_enumerate_entries(AppDBEnumerateCb cb, void *data);

/**
 * @brief Check whether an install id is in the database.
 *
 * @param app_id Install id.
 * @return true if it exists.
 */
bool app_db_exists_install_id(AppInstallId app_id);

/**
 * @brief Initialize the app database.
 *
 * Scans the database to pick the next unused install id.
 */
void app_db_init(void);

/**
 * @brief Insert or replace a record in the app database.
 *
 * The key is the app UUID and the value an ::AppDBEntry. A new app gets the next unused install
 * id; an existing one keeps its id, and an in-progress fetch of it is cancelled. Notifies the
 * app install manager.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the app database.
 *
 * The key is the app UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int app_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the app database.
 *
 * The key is the app UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the app database.
 *
 * The key is the app UUID. Notifies the app install manager.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the app database.
 *
 * Also cancels any app fetch and clears the app install manager state.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_flush(void);

/**
 * @brief Compact the settings file backing the app database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t app_db_compact(void);

/**
 * @brief Get the next install id to be assigned, for tests.
 *
 * @return Install id.
 */
AppInstallId app_db_check_next_unique_id(void);

/** @} */
