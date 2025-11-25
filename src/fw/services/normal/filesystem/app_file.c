/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_file.h"

#include <string.h>

#include "system/passert.h"
#include "resource/resource_storage.h"

void app_file_name_make(char * restrict buffer, size_t buffer_len,
                        AppInstallId app_id, const char * restrict suffix,
                        size_t suffix_len) {
  PBL_ASSERTN(buffer_len > APP_FILE_NAME_PREFIX_LENGTH + suffix_len);

  buffer[0] = '@';

  uint32_t unsigned_id = (uint32_t)app_id;
  for (int i = 8; i >= 1; --i) {
    uint8_t nybble = unsigned_id & 0xf;
    if (nybble < 0xa) {
      buffer[i] = '0' + nybble;
    } else {
      buffer[i] = 'a' + (nybble - 0xa);
    }
    unsigned_id >>= 4;
  }

  buffer[9] = '/';

  memcpy(&buffer[10], suffix, suffix_len);
  buffer[APP_FILE_NAME_PREFIX_LENGTH + suffix_len] = '\0';
}

//! Checks whether the given filename is an app file.
bool is_app_file_name(const char *filename) {
  bool answer = true;
  answer = answer && strlen(filename) > APP_FILE_NAME_PREFIX_LENGTH;
  answer = answer && filename[0] == '@' && filename[9] == '/';
  for (int i = 1; answer && i <= 8; ++i) {
    char c = filename[i];
    answer = answer && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }
  return answer;
}

//! Checks whether the given filename is an app resource file (suffix = "res")
bool is_app_resource_file_name(const char *filename) {
  return is_app_file_name(filename) &&
         !strcmp(filename + APP_FILE_NAME_PREFIX_LENGTH, APP_RESOURCES_FILENAME_SUFFIX);
}

//! Parses an app-file name to get the AppInstallId.
//! Assumes the file is indeed an app-file
AppInstallId app_file_parse_app_id(const char *filename) {
  return (AppInstallId)strtol(filename + 1, NULL, 16); // + 1 to skip the initial '@'
}

//! Parses an app-file name to get the AppInstallId.
//!
//! @returns INSTALL_ID_INVALID if the filename is not an app-file.
AppInstallId app_file_get_app_id(const char *filename) {
  if (!is_app_file_name(filename)) {
    return INSTALL_ID_INVALID;
  }
  return app_file_parse_app_id(filename);
}
