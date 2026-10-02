/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "pbl/util/uuid.h"
#include "system/status_codes.h"

/**
 * @defgroup services_persist Persist
 * @ingroup services
 * @brief Per-app persistent key-value stores.
 *
 * A store is a SettingsFile named after the app UUID. An app and its worker share one store,
 * file handle and SettingsFile state. The file is created lazily on first access.
 *
 * The SettingsFile is not reentrant: access it only between
 * persist_service_lock_and_get_store() and persist_service_unlock_store().
 *
 * @code{.c}
 * SettingsFile *file = persist_service_lock_and_get_store(&app_uuid);
 * status_t rv = settings_file_set(file, &key, sizeof(key), &value, sizeof(value));
 * persist_service_unlock_store(file);
 * @endcode
 * @{
 */

/** @brief Settings file backing a store. */
typedef struct SettingsFile SettingsFile;

/**
 * @brief Initialize the persist service.
 *
 * Called once at boot. Migrates and cleans up files in legacy naming schemes.
 */
void persist_service_init(void);

/**
 * @brief Get the per-app storage capacity.
 *
 * @return Capacity in bytes.
 */
size_t persist_service_get_max_size(void);

/**
 * @brief Lock the persist service and get the store of an app.
 *
 * Opens or creates the file on first use. The app must have been opened with
 * persist_service_client_open(). The lock is held until persist_service_unlock_store().
 *
 * @param uuid App UUID.
 * @return Store of the app.
 */
SettingsFile *persist_service_lock_and_get_store(const Uuid *uuid);

/**
 * @brief Unlock a store obtained with persist_service_lock_and_get_store().
 *
 * @param store Store to unlock.
 */
void persist_service_unlock_store(SettingsFile *store);

/**
 * @brief Register a process using the store of an app.
 *
 * Called during process startup.
 *
 * @param uuid App UUID.
 */
void persist_service_client_open(const Uuid *uuid);

/**
 * @brief Unregister a process using the store of an app.
 *
 * Called after the process exits. The store is closed and released when its last user leaves.
 *
 * @param uuid App UUID.
 */
void persist_service_client_close(const Uuid *uuid);

/**
 * @brief Delete the persist file of an app.
 *
 * @param uuid App UUID.
 * @return Status of the file removal.
 */
status_t persist_service_delete_file(const Uuid *uuid);

/** @} */
