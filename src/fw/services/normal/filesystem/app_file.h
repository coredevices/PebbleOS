/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Consistent naming of per-app files.
//!
//! All files which are specific to an app are named with a consistent scheme
//! which identifies the files as belonging to the app. This is done by
//! prefixing the filename with a string based on the AppInstallId. Filenames
//! take the format printf("@%08x/%s", (uint32_t)app_id, suffix) to form a
//! pseudo-directory structure.
//!
//! The prefix is fixed-length to make it simple to generate, parse and
//! identify.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "process_management/app_install_types.h"

// The suffix starts at offset 10 in the filename
// '@' + "XXXXXXXX" + '/' : (1 + 8 + 1 = 10)
#define APP_FILE_NAME_PREFIX_LENGTH (10)

//! Make an app-file name from the given app_id and suffix string.
//!
//! @param suffix_len strlen(suffix)
//!
//! @note buffer_len must be > APP_FILE_NAME_PREFIX_LENGTH + suffix_len to fit
//! the full file name including NULL-terminator.
void app_file_name_make(char * restrict buffer, size_t buffer_len,
                        AppInstallId app_id, const char * restrict suffix,
                        size_t suffix_len);

//! Checks whether the given filename is an app file.
bool is_app_file_name(const char *filename);

//! Checks whether the given filename is an app resource file (suffix = "res")
bool is_app_resource_file_name(const char *filename);

//! Parses an app-file name to get the AppInstallId.
//! Assumes the file is indeed an app-file
AppInstallId app_file_parse_app_id(const char *filename);

//! Parses an app-file name to get the AppInstallId.
//!
//! @returns INSTALL_ID_INVALID if the filename is not an app-file.
AppInstallId app_file_get_app_id(const char *filename);
