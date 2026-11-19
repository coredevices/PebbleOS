/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/drivers/flash.h>
#include <pbl/fs/fs.h>
#include <pbl/kernel/assert.h>
#include <pbl/logging/logging.h>
#include <pbl/services/process_management/app_file.h>
#include <pbl/services/process_management/app_storage.h>
#include <pbl/util/build_id.h>

#include <process_management/pebble_process_md.h>
#include <resource/resource_storage.h>

PBL_LOG_MODULE_DECLARE(service_process_management, CONFIG_SERVICE_PROCESS_MANAGEMENT_LOG_LEVEL);

//! Whether a platform info read from a binary is one this firmware understands.
static bool prv_platform_info_is_valid(const PebblePlatformInfo *platform_info) {
  return (platform_info->version == PROCESS_PLATFORM_INFO_VERSION) &&
         (platform_info->length >= sizeof(PebblePlatformInfo)) &&
         ((platform_info->flags & PROCESS_PLATFORM_INFO_FLAG_FLEXIBLE) ||
          ((platform_info->display_width > 0) && (platform_info->display_height > 0)));
}

AppStorageGetAppInfoResult app_storage_get_process_info(PebbleProcessInfo *app_info,
                                                        uint8_t *build_id_out,
                                                        PebblePlatformInfo *platform_info_out,
                                                        AppInstallId app_id, PebbleTask task_type) {
  char process_name[APP_FILENAME_MAX_LENGTH];
  app_storage_get_file_name(process_name, sizeof(process_name), app_id, task_type);
  struct pbl_fs_file file;
  if (pbl_fs_open(&file, process_name, PBL_FS_O_READ) < 0) {
    return (GET_APP_INFO_COULD_NOT_READ_FORMAT);
  }
  if (pbl_fs_read(&file, (uint8_t *)app_info, sizeof(PebbleProcessInfo)) !=
      sizeof(PebbleProcessInfo)) {
    pbl_fs_close(&file);
    return (GET_APP_INFO_COULD_NOT_READ_FORMAT);
  }
  if (platform_info_out) {
    *platform_info_out = (PebblePlatformInfo){};
  }
  if (build_id_out || platform_info_out) {
    const uint8_t padding_size = sizeof(PebbleProcessInfo) % 4;
    // The note.gnu.build-id section seems to have a hard-coded word-alignment requirement...
    // The platform info, when there is one, follows the note.
    uint8_t buffer[padding_size + BUILD_ID_TOTAL_EXPECTED_LEN + sizeof(PebblePlatformInfo)];
    const ElfExternalNote *note = (const ElfExternalNote *)(buffer + padding_size);
    const int note_end = padding_size + BUILD_ID_TOTAL_EXPECTED_LEN;
    const int result = pbl_fs_read(&file, buffer, sizeof(buffer));
    const bool has_build_id = (result >= note_end) && build_id_contains_gnu_build_id(note);
    if (build_id_out) {
      if (has_build_id) {
        memcpy(build_id_out, note->data + note->name_length, BUILD_ID_EXPECTED_LEN);
      } else {
        memset(build_id_out, 0, BUILD_ID_EXPECTED_LEN);
      }
    }
    if (platform_info_out && has_build_id && (result == (int)sizeof(buffer)) &&
        (app_info->flags & PROCESS_INFO_HAS_PLATFORM_INFO)) {
      PebblePlatformInfo platform_info;
      memcpy(&platform_info, buffer + note_end, sizeof(platform_info));
      if (prv_platform_info_is_valid(&platform_info)) {
        *platform_info_out = platform_info;
      }
    }
  }
  pbl_fs_close(&file);

  if (strncmp("PBLAPP", app_info->header, sizeof(app_info->header)) != 0) {
    // there isn't a valid app in the bank
    return GET_APP_INFO_COULD_NOT_READ_FORMAT;
  }

  const bool is_sdk_compatible =
      (app_info->sdk_version.major == PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR &&
       app_info->sdk_version.minor <= PROCESS_INFO_CURRENT_SDK_VERSION_MINOR);

  if (is_sdk_compatible == false) {
    PBL_LOG_WRN("App requires support for SDK version (%u.%u), we only support version (%u.%u).",
                app_info->sdk_version.major, app_info->sdk_version.minor,
                PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR, PROCESS_INFO_CURRENT_SDK_VERSION_MINOR);

    // The app's is built with an SDK that is incompatible with the running fw
    return GET_APP_INFO_INCOMPATIBLE_SDK;
  }

  return GET_APP_INFO_SUCCESS;
}

void app_storage_delete_app(AppInstallId id) {
  PBL_ASSERTN(id > 0);
  char process_name[APP_FILENAME_MAX_LENGTH];

  // remove worker
  app_storage_get_file_name(process_name, sizeof(process_name), id, PebbleTask_Worker);
  pbl_fs_unlink(process_name);
  // remove app too
  app_storage_get_file_name(process_name, sizeof(process_name), id, PebbleTask_App);
  pbl_fs_unlink(process_name);
  // remove resources
  resource_storage_clear(id);
}

bool app_storage_app_exists(AppInstallId id) {
  PBL_ASSERTN(id > 0);
  char process_name[APP_FILENAME_MAX_LENGTH];

  // check app binary first
  app_storage_get_file_name(process_name, sizeof(process_name), id, PebbleTask_App);
  struct pbl_fs_file file;
  if (pbl_fs_open(&file, process_name, PBL_FS_O_READ) < 0) {
    return false;
  }
  pbl_fs_close(&file);

  // now check resource bank
  return resource_storage_check((ResAppNum)id, 0, nullptr);
}

void app_storage_get_file_name(char *name, size_t buf_length, AppInstallId app_id,
                               PebbleTask task) {
  const char *task_str = (task == PebbleTask_App) ? APP_FILE_NAME_SUFFIX : WORKER_FILE_NAME_SUFFIX;
  size_t task_str_len =
      (task == PebbleTask_App) ? strlen(APP_FILE_NAME_SUFFIX) : strlen(WORKER_FILE_NAME_SUFFIX);
  app_file_path_make(name, buf_length, app_id, task_str, task_str_len);
}
