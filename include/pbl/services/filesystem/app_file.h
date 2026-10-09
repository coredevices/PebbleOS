/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <process_management/app_install_types.h>

/**
 * @defgroup services_filesystem_app_file App files
 * @ingroup services_filesystem
 * @brief Consistent naming of per-app files.
 *
 * Files belonging to an app are named with a fixed-length prefix derived from its
 * @c AppInstallId, forming a pseudo-directory: `printf("@%08x/%s", (uint32_t)app_id, suffix)`
 * with lowercase hex digits. The fixed length keeps the prefix simple to generate, parse and
 * identify.
 * @{
 */

/** @brief Length of the `@XXXXXXXX/` prefix; the suffix starts at this offset. */
#define APP_FILE_NAME_PREFIX_LENGTH (10)

/**
 * @brief Make an app file name.
 *
 * @param[out] buffer Buffer to write the NUL-terminated file name into.
 * @param buffer_len Size of @p buffer in bytes, greater than
 * @ref APP_FILE_NAME_PREFIX_LENGTH + @p suffix_len (asserted).
 * @param app_id App install id.
 * @param suffix Suffix string.
 * @param suffix_len Length of @p suffix, without the NUL terminator.
 */
void app_file_name_make(char *restrict buffer, size_t buffer_len, AppInstallId app_id,
                        const char *restrict suffix, size_t suffix_len);

/**
 * @brief Check whether a file name is an app file name.
 *
 * @param filename File name.
 * @return true if it has a valid app file prefix followed by a non-empty suffix.
 */
bool is_app_file_name(const char *filename);

/**
 * @brief Check whether a file name is an app resource file name (suffix @c "res").
 *
 * @param filename File name.
 * @return true for app resource files.
 */
bool is_app_resource_file_name(const char *filename);

/**
 * @brief Get the app install id from an app file name, without validating it.
 *
 * @param filename App file name.
 * @return App install id.
 */
AppInstallId app_file_parse_app_id(const char *filename);

/**
 * @brief Get the app install id from a file name.
 *
 * @param filename File name.
 * @return App install id, or @c INSTALL_ID_INVALID if @p filename is not an app file name.
 */
AppInstallId app_file_get_app_id(const char *filename);

/** @} */
