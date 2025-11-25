/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "drivers/crc.h"
#include "drivers/flash.h"
#include "flash_region.h"
#include "git_version.auto.h"
#include "system/firmware_storage.h"
#include "system/passert.h"
#include "version.h"

//! The linker inserts the build id as an "elf external note" structure:
struct ElfExternalNote {
  uint32_t name_length;
  uint32_t data_length;
  uint32_t type; // NT_GNU_BUILD_ID = 3
  uint8_t data[]; // concatenated name ('GNU') + data (build id)
};

//! This symbol and its contents are provided by the linker script, see the
//! .note.gnu.build-id section in src/fw/stm32f2xx_flash_fw.ld
extern const struct ElfExternalNote TINTIN_BUILD_ID;

const FirmwareMetadata TINTIN_METADATA __attribute__ ((section (".pbl_fw_version"))) = {
  .version_timestamp = GIT_TIMESTAMP,
  .version_tag = GIT_TAG,
  .version_short = GIT_REVISION,

#ifdef RECOVERY_FW
  .is_recovery_firmware = true,
#else
  .is_recovery_firmware = false,
#endif

#if defined(BOARD_BIGBOARD)
  .hw_platform = FirmwareMetadataPlatformPebbleOneBigboard,
#elif defined(BOARD_BB2)
  .hw_platform = FirmwareMetadataPlatformPebbleOneBigboard2,
#elif defined(BOARD_V2_0)
  .hw_platform = FirmwareMetadataPlatformPebbleTwoPointZero,
#elif defined(BOARD_V1_5)
  .hw_platform = FirmwareMetadataPlatformPebbleOnePointFive,
#elif defined(BOARD_EV2_4)
  .hw_platform = FirmwareMetadataPlatformPebbleOneEV2_4,
#else
  .hw_platform = FirmwareMetadataPlatformUnknown,
#endif

  .metadata_version = FW_METADATA_CURRENT_STRUCT_VERSION,
};

bool version_copy_running_fw_metadata(FirmwareMetadata *out_metadata) {
  if (out_metadata == NULL) {
    return false;
  }
  memcpy(out_metadata, &TINTIN_METADATA, sizeof(FirmwareMetadata));
  return true;
}

static bool prv_version_copy_flash_fw_metadata(FirmwareMetadata *out_metadata, uint32_t flash_address) {
  if (out_metadata == NULL) {
    return false;
  }

  FirmwareDescription firmware_description = firmware_storage_read_firmware_description(flash_address);
  if (!firmware_storage_check_valid_firmware_description(&firmware_description)) {
    memset(out_metadata, 0, sizeof(FirmwareMetadata));
    return false;
  }
  // The FirmwareMetadata is stored at the end of the binary:
  uint32_t offset = firmware_description.description_length + firmware_description.firmware_length - sizeof(FirmwareMetadata);
  flash_read_bytes((uint8_t*)out_metadata, flash_address + offset, sizeof(FirmwareMetadata));
  return true;
}

bool version_copy_recovery_fw_metadata(FirmwareMetadata *out_metadata) {
  return prv_version_copy_flash_fw_metadata(out_metadata, FLASH_REGION_SAFE_FIRMWARE_BEGIN);
}

bool version_copy_update_fw_metadata(FirmwareMetadata *out_metadata) {
  return prv_version_copy_flash_fw_metadata(out_metadata, FLASH_REGION_FIRMWARE_SCRATCH_BEGIN);
}

bool version_copy_recovery_fw_version(char* dest, const int dest_len_bytes) {
  FirmwareMetadata out_metadata;
  bool success = version_copy_recovery_fw_metadata(&out_metadata);
  if (success) {
    strncpy(dest, out_metadata.version_tag, dest_len_bytes);
  }
  return success;
}

bool version_is_prf_installed(void) {
  FirmwareDescription firmware_description =
      firmware_storage_read_firmware_description(FLASH_REGION_SAFE_FIRMWARE_BEGIN);
  if (!firmware_storage_check_valid_firmware_description(&firmware_description)) {
    return false;
  }

  uint32_t flash_address = FLASH_REGION_SAFE_FIRMWARE_BEGIN + firmware_description.description_length;
  uint32_t crc = crc_calculate_flash(flash_address, firmware_description.firmware_length);
  return crc == firmware_description.checksum;
}

const uint8_t * version_get_build_id(size_t *out_len) {
  if (out_len) {
    *out_len = TINTIN_BUILD_ID.data_length;
  }
  return &TINTIN_BUILD_ID.data[TINTIN_BUILD_ID.name_length];
}

void version_copy_build_id_hex_string(char *buffer, size_t buffer_bytes_left) {
  size_t build_id_bytes_left;
  const uint8_t *build_id = version_get_build_id(&build_id_bytes_left);
  while (buffer_bytes_left >= 3 /* 2 hex digits, plus zero terminator */
         && build_id_bytes_left > 0) {
    snprintf(buffer, buffer_bytes_left, "%02x", *build_id);

    buffer += 2;
    buffer_bytes_left -= 2;

    build_id += 1;
    build_id_bytes_left -= 1;
  }
}

static void version_fw_version_to_major_minor(unsigned int *major, unsigned int *minor,
  char *version_str) {
  // read in the two X's (vX.X)
  sscanf(version_str, "v%u.%u", major, minor);
}

//! Compares its two arguments for order. Returns a negative integer, zero, or a positive integer
//! if the first argument is less than, equal to, or greater than the second.
static int8_t prv_version_compare_fw_version_tags(char *fw1_version, char *fw2_version) {
  unsigned int major1, minor1, major2, minor2;
  version_fw_version_to_major_minor(&major1, &minor1, fw1_version);
  version_fw_version_to_major_minor(&major2, &minor2, fw2_version);

  if (major1 != major2) { // do the major versions differ?
    return (major1 - major2);
  }

  if (minor1 != minor2) { // do the minor versions differ?
    return (minor1 - minor2);
  }

  return (0); // versions are the same
}

bool version_fw_downgrade_detected(void) {
  FirmwareMetadata running_meta_data, update_meta_data;
  version_copy_running_fw_metadata(&running_meta_data);
  version_copy_update_fw_metadata(&update_meta_data);

  int rv = prv_version_compare_fw_version_tags(update_meta_data.version_tag,
    running_meta_data.version_tag);

  // return true is the new firmware to be updated to is a version less than the old one.
  return (rv < 0);
}
