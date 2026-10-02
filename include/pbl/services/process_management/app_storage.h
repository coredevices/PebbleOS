/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"
#include "flash_region/flash_region.h"
#include "process_management/pebble_process_info.h"
#include "process_management/app_install_types.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @defgroup services_process_management Process management
 * @ingroup services
 * @brief Storage of installed app and worker binaries.
 *
 * App and worker binaries are stored in PFS as app files (see @ref services_filesystem_app_file)
 * with the suffixes @ref APP_FILE_NAME_SUFFIX and @ref WORKER_FILE_NAME_SUFFIX; resources are
 * kept separately by the resource storage.
 * @{
 */

/** @brief App file suffix of app binaries. */
#define APP_FILE_NAME_SUFFIX "app"
/** @brief App file suffix of worker binaries. */
#define WORKER_FILE_NAME_SUFFIX "worker"

/** @brief Number of app banks of the legacy put_bytes protocol. */
#define MAX_APP_BANKS 8
/** @brief Size of a buffer able to hold an app or worker binary file name. */
#define APP_FILENAME_MAX_LENGTH 32

/** @brief Result of app_storage_get_process_info(). */
typedef enum AppStorageGetAppInfoResult {
  /** Metadata read and compatible with the running firmware. */
  GET_APP_INFO_SUCCESS,
  /** The binary is missing, unreadable or not a Pebble app. */
  GET_APP_INFO_COULD_NOT_READ_FORMAT,
  /** The app was built with an SDK newer than the firmware supports. */
  GET_APP_INFO_INCOMPATIBLE_SDK,
} AppStorageGetAppInfoResult;

/**
 * @brief Read and check the metadata of a stored app or worker binary.
 *
 * @param[out] app_info Metadata read from flash.
 * @param[out] build_id_out Buffer of at least @c BUILD_ID_EXPECTED_LEN bytes for the GNU build ID
 * of the executable, zero-filled if there is none. May be NULL.
 * @param app_id App install id.
 * @param task @c PebbleTask_App or @c PebbleTask_Worker.
 * @return Result of the check.
 */
AppStorageGetAppInfoResult app_storage_get_process_info(PebbleProcessInfo *app_info,
                                                        uint8_t *build_id_out, AppInstallId app_id,
                                                        PebbleTask task);

/**
 * @brief Remove the app, worker and resource files of an app.
 *
 * @param id App install id, greater than 0.
 */
void app_storage_delete_app(AppInstallId id);

/**
 * @brief Check whether the app binary and resources of an app are stored.
 *
 * @param id App install id, greater than 0.
 * @return true if both exist.
 */
bool app_storage_app_exists(AppInstallId id);

/**
 * @brief Get the file name of an app or worker binary.
 *
 * @param[out] name Buffer for the NUL-terminated file name.
 * @param buf_length Size of @p name, see @ref APP_FILENAME_MAX_LENGTH.
 * @param app_id App install id.
 * @param task @c PebbleTask_App or @c PebbleTask_Worker.
 */
void app_storage_get_file_name(char *name, size_t buf_length, AppInstallId app_id, PebbleTask task);

/**
 * @brief Compute the size of the process image plus its relocation table.
 *
 * @param info Process metadata.
 * @param[out] load_size_out Load size in bytes, set on success.
 * @return true on success, false if the size overflows.
 */
bool app_storage_get_process_load_size(const PebbleProcessInfo *info, size_t *load_size_out);

/** @} */
