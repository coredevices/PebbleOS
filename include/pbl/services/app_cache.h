/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <process_management/app_install_types.h>
#include <system/status_codes.h>

/**
 * @defgroup services_app_cache App cache
 * @ingroup services
 * @brief Tracks the applications whose binaries are stored on the watch.
 *
 * Each entry records an app's size, install date, last launch and launch count, persisted in a
 * settings file. When storage runs low, the entries with the lowest priority (least recently
 * installed or launched) are evicted and their binaries deleted; the default watchface and worker
 * and the quick launch apps are only evicted as a last resort. Removing an entry deletes the app's
 * binaries and emits a @c PEBBLE_APP_CACHE_EVENT.
 * @{
 */

/**
 * @brief Initialize the app cache.
 *
 * Deletes leftover app binaries when no cache file exists and purges orphaned resource files.
 */
void app_cache_init(void);

/**
 * @brief Add an entry for a newly installed app.
 *
 * Schedules an eviction on KernelBackground if free space is now too low.
 *
 * @param app_id App to add.
 * @param total_size Size of the app's binaries, in bytes.
 * @return S_SUCCESS or an error from the settings file.
 */
status_t app_cache_add_entry(AppInstallId app_id, uint32_t total_size);

/**
 * @brief Remove an app's entry and delete its binaries.
 *
 * @param app_id App to remove.
 * @return S_SUCCESS or an error from the settings file.
 */
status_t app_cache_remove_entry(AppInstallId app_id);

/**
 * @brief Check whether an app has an entry in the cache.
 *
 * An entry whose binaries are missing from storage is removed.
 *
 * @param app_id App to look up.
 * @return true if the entry exists and the app's binaries are stored.
 */
bool app_cache_entry_exists(AppInstallId app_id);

/**
 * @brief Record a launch of an app.
 *
 * Updates the last launch time and launch count. If the app has no entry, its binaries are
 * deleted.
 *
 * @param app_id Launched app.
 * @return S_SUCCESS or an error from the settings file.
 */
status_t app_cache_app_launched(AppInstallId app_id);

/**
 * @brief Evict apps until at least @p bytes_needed bytes of binaries are freed.
 *
 * @param bytes_needed Number of bytes to free.
 * @retval S_SUCCESS Success.
 * @retval E_INVALID_ARGUMENT @p bytes_needed is 0.
 * @return Other errors come from the settings file.
 */
status_t app_cache_free_up_space(uint32_t bytes_needed);

/**
 * @brief Clear the whole cache and delete all cached app binaries.
 *
 * Must be called from KernelBackground.
 */
void app_cache_flush(void);

/** @} */
