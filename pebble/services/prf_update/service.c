/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

#include <pebbleos/firmware_metadata.h>
#include <system/bootbits.h>
#include <system/firmware_storage.h>

PBL_LOG_MODULE_DEFINE(service_prf_update, CONFIG_SERVICE_PRF_UPDATE_LOG_LEVEL);

static void prv_finish(void) {
  boot_bit_clear(BOOT_BIT_NEW_PRF_AVAILABLE | BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_ONE |
                 BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_TWO);
}

// Don't allow PRF updating when we're in PRF
#ifndef CONFIG_RECOVERY_FW
typedef enum PrfUpdateResult {
  //! The PRF slot holds the staged firmware.
  PrfUpdateResultDone,
  //! The staged image can never be installed.
  PrfUpdateResultRejected,
  //! Copying to the PRF slot failed.
  PrfUpdateResultCopyFailed,
} PrfUpdateResult;

// Whether the image ends with the metadata of a recovery firmware for this board
static bool prv_is_prf_for_this_board(uint32_t total_length) {
  FirmwareMetadata metadata;

  if (total_length < sizeof(metadata)) {
    return false;
  }

  if (pbl_flash_partition_read(PART_FIRMWARE_DEST, total_length - sizeof(metadata), &metadata,
                               sizeof(metadata)) != 0) {
    return false;
  }

  return metadata.metadata_version == FW_METADATA_CURRENT_STRUCT_VERSION &&
         metadata.is_recovery_firmware &&
         (FIRMWARE_METADATA_HW_PLATFORM == FirmwareMetadataPlatformUnknown ||
          metadata.hw_platform == FIRMWARE_METADATA_HW_PLATFORM);
}

static bool prv_copy(uint32_t total_length) {
  uint8_t buffer[512];

  PBL_LOG_DBG("Erasing previous PRF...");
  if (pbl_flash_partition_erase(PART_SAFE_FIRMWARE, 0U, total_length) != 0) {
    return false;
  }

  PBL_LOG_DBG("Copying PRF from scratch to the PRF slot");
  for (uint32_t offset = 0U; offset < total_length; offset += sizeof(buffer)) {
    const uint32_t chunk_size = PBL_MIN(sizeof(buffer), (total_length - offset));

    if (pbl_flash_partition_read(PART_FIRMWARE_DEST, offset, buffer, chunk_size) != 0 ||
        pbl_flash_partition_write(PART_SAFE_FIRMWARE, offset, buffer, chunk_size) != 0) {
      return false;
    }
  }

  return true;
}

// Copies the staged recovery firmware to the PRF slot
static PrfUpdateResult prv_do_update(void) {
  bool verified;

#ifndef CONFIG_PBLBOOT
  FirmwareDescription description =
      firmware_storage_read_firmware_description(PART_FIRMWARE_DEST_OFFSET);

  if (!firmware_storage_check_valid_firmware_description(PART_FIRMWARE_DEST_OFFSET, &description)) {
    PBL_LOG_WRN("Invalid recovery firmware CRC in SPI flash!");
    return PrfUpdateResultRejected;
  }

  const uint32_t total_length = description.description_length + description.firmware_length;
#else
  FirmwareHeader header = firmware_storage_read_firmware_header(PART_FIRMWARE_DEST_OFFSET);
  if (!firmware_storage_check_valid_firmware_header(PART_FIRMWARE_DEST_OFFSET, &header)) {
    PBL_LOG_WRN("Invalid recovery firmware CRC in SPI flash!");
    return PrfUpdateResultRejected;
  }

  const uint32_t total_length = header.fw_start + header.fw_length;
#endif

  if (total_length > PART_SAFE_FIRMWARE_SIZE || total_length > PART_FIRMWARE_DEST_SIZE) {
    PBL_LOG_WRN("Recovery firmware does not fit the PRF slot!");
    return PrfUpdateResultRejected;
  }

  if (!prv_is_prf_for_this_board(total_length)) {
    PBL_LOG_WRN("Staged firmware is not a recovery firmware for this board!");
    return PrfUpdateResultRejected;
  }

  PBL_LOG_INFO("Updating PRF!");
  (void)pbl_flash_unprotect(FLASH);

  verified = prv_copy(total_length);

  // Check what landed in the slot, not what was staged
#ifndef CONFIG_PBLBOOT
  const FirmwareDescription copied =
      firmware_storage_read_firmware_description(PART_SAFE_FIRMWARE_OFFSET);
  verified = verified && memcmp(&copied, &description, sizeof(copied)) == 0 &&
             firmware_storage_check_valid_firmware_description(PART_SAFE_FIRMWARE_OFFSET, &copied);
#else
  const FirmwareHeader copied = firmware_storage_read_firmware_header(PART_SAFE_FIRMWARE_OFFSET);
  verified = verified && memcmp(&copied, &header, sizeof(copied)) == 0 &&
             firmware_storage_check_valid_firmware_header(PART_SAFE_FIRMWARE_OFFSET, &copied);
#endif

  (void)pbl_flash_partition_protect(PART_SAFE_FIRMWARE);

  if (!verified) {
    PBL_LOG_WRN("Recovery firmware copy failed!");
    return PrfUpdateResultCopyFailed;
  }

  PBL_LOG_DBG("Done!");

  return PrfUpdateResultDone;
}

// Keeps the request for another try on the next boot, up to three tries in all
static void prv_copy_failed(void) {
  if (!boot_bit_test(BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_ONE)) {
    boot_bit_set(BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_ONE);
  } else if (!boot_bit_test(BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_TWO)) {
    boot_bit_set(BOOT_BIT_PRF_UPDATE_FAIL_STRIKE_TWO);
  } else {
    PBL_LOG_ERR("Recovery firmware copy failed 3 times, giving up");
    prv_finish();
    return;
  }

  PBL_LOG_WRN("Retrying the recovery firmware copy on the next boot");
}
#endif

void check_prf_update(void) {
  if (!boot_bit_test(BOOT_BIT_NEW_PRF_AVAILABLE)) {
    return;
  }

#ifndef CONFIG_RECOVERY_FW
  if (prv_do_update() == PrfUpdateResultCopyFailed) {
    prv_copy_failed();
    return;
  }
#endif

  prv_finish();
}
