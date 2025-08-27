/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applib/app_watch_info.h"
#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "system/firmware_storage.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "util/build_id.h"
#include "util/string.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "version.h"

#include "git_version.auto.h"

//! This symbol and its contents are provided by the linker script, see the
//! .note.gnu.build-id section in src/fw/stm32f2xx_flash_fw.ld
extern const ElfExternalNote TINTIN_BUILD_ID;

const FirmwareMetadata TINTIN_METADATA SECTION(".pbl_fw_version") = {
  .version_timestamp = GIT_TIMESTAMP,
  .version_tag = GIT_TAG,
  .version_short = GIT_REVISION,

  .is_recovery_firmware = FIRMWARE_METADATA_IS_RECOVERY_FIRMWARE,
  .is_ble_firmware = false,
  .reserved = 0,

  .hw_platform = FIRMWARE_METADATA_HW_PLATFORM,

  .metadata_version = FW_METADATA_CURRENT_STRUCT_VERSION,
};

bool version_copy_running_fw_metadata(FirmwareMetadata *out_metadata) {
  if (out_metadata == NULL) {
    return false;
  }
  memcpy(out_metadata, &TINTIN_METADATA, sizeof(FirmwareMetadata));
  return true;
}

static bool prv_version_copy_flash_fw_metadata(FirmwareMetadata *out_metadata,
                                               uint32_t flash_address, bool check_crc) {
  FirmwareDescription firmware_description =
      firmware_storage_read_firmware_description(flash_address);

  if (check_crc &&
      !firmware_storage_check_valid_firmware_description(flash_address,
                                                         &firmware_description)) {
    *out_metadata = (FirmwareMetadata){};
    return false;
  }

  // The FirmwareMetadata is stored at the end of the binary
  const uint32_t metadata_offset = flash_address +
                                   firmware_description.description_length +
                                   firmware_description.firmware_length -
                                   sizeof(FirmwareMetadata);

  flash_read_bytes((uint8_t*)out_metadata, metadata_offset, sizeof(FirmwareMetadata));

  return true;
}

bool version_copy_recovery_fw_metadata(FirmwareMetadata *out_metadata) {
  const bool check_crc = true;
  return prv_version_copy_flash_fw_metadata(out_metadata, FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                                            check_crc);
}

bool version_copy_update_fw_metadata(FirmwareMetadata *out_metadata) {
  const bool check_crc = false;
  uint32_t addr = FLASH_REGION_FIRMWARE_SLOT_1_BEGIN;

#ifdef SWAP_OFFSET_UPGRADE
  addr += SUBSECTOR_SIZE_BYTES;
#endif

  return prv_version_copy_flash_fw_metadata(out_metadata, addr, check_crc);
}

bool version_copy_recovery_fw_version(char* dest, const int dest_len_bytes) {
  FirmwareMetadata out_metadata;
  const bool check_crc = false;
  bool success = prv_version_copy_flash_fw_metadata(&out_metadata,
                                                    FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                                                    check_crc);
  if (success) {
    strncpy(dest, out_metadata.version_tag, dest_len_bytes);
  }
  return success;
}

bool version_is_prf_installed(void) {
  FirmwareDescription firmware_description =
      firmware_storage_read_firmware_description(FLASH_REGION_SAFE_FIRMWARE_BEGIN);

  return firmware_storage_check_valid_firmware_description(FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                                                           &firmware_description);
}

const uint8_t * version_get_build_id(size_t *out_len) {
  if (out_len) {
    *out_len = TINTIN_BUILD_ID.data_length;
  }
  PBL_ASSERTN(TINTIN_BUILD_ID.data_length == BUILD_ID_EXPECTED_LEN);

  return &TINTIN_BUILD_ID.data[TINTIN_BUILD_ID.name_length];
}

void version_copy_build_id_hex_string(char *buffer, size_t buffer_bytes_left,
                                      const ElfExternalNote *elf_build_id) {
  size_t build_id_bytes_left = elf_build_id->data_length;
  const uint8_t *build_id = &elf_build_id->data[elf_build_id->name_length];
  byte_stream_to_hex_string(buffer, buffer_bytes_left, build_id,
                            build_id_bytes_left, false);
}

void version_copy_current_build_id_hex_string(char *buffer, size_t buffer_bytes_left) {
  version_copy_build_id_hex_string(buffer, buffer_bytes_left, &TINTIN_BUILD_ID);
}

void version_get_major_minor_patch(unsigned int *major, unsigned int *minor,
                                   char const **patch_ptr) {
  *major = GIT_MAJOR_VERSION;
  *minor = GIT_MINOR_VERSION;
  *patch_ptr = GIT_PATCH_VERBOSE_STRING;
}
