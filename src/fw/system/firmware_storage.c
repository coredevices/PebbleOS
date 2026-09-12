/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "firmware_storage.h"

#include <pbl/drivers/flash.h>
#include "flash_region/flash_region.h"
#include <pbl/logging/logging.h>

#ifndef CONFIG_PBLBOOT
FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address) {
  FirmwareDescription firmware_description;
  pbl_flash_read(FLASH, firmware_start_address, (uint8_t *)&firmware_description,
                 sizeof(FirmwareDescription));

  return firmware_description;
}

bool firmware_storage_check_valid_firmware_description(
    uint32_t start_address, const FirmwareDescription *firmware_description) {

  if (firmware_description->description_length != sizeof(FirmwareDescription)) {
    // Corrupted description
    return false;
  }

  // Log around this operation, as it can take some time (hundreds of ms)
  PBL_LOG_DBG("CRCing recovery...");

  start_address += sizeof(FirmwareDescription);
  const uint32_t calculated_crc =
      pbl_flash_crc32(FLASH, start_address, firmware_description->firmware_length);

  PBL_LOG_DBG("CRCing recovery... done");

  return calculated_crc == firmware_description->checksum;
}
#else
FirmwareHeader firmware_storage_read_firmware_header(uint32_t address) {
  FirmwareHeader header;
  pbl_flash_read(FLASH, address, (uint8_t*) &header, sizeof(FirmwareHeader));
  return header;
}

bool firmware_storage_check_valid_firmware_header(
    uint32_t address, const FirmwareHeader* header) {

  if (header->magic != FIRMWARE_HEADER_MAGIC ||
      header->header_length != sizeof(FirmwareHeader)) {
    // Corrupted header
    return false;
  }

  // Log around this operation, as it can take some time (hundreds of ms)
  PBL_LOG_DBG("CRCing recovery...");

  const uint32_t calculated_crc =
      pbl_flash_crc32(FLASH, address + header->fw_start, header->fw_length);

  PBL_LOG_DBG("CRCing recovery... done");

  return calculated_crc == header->fw_crc;
}

void firmware_storage_invalidate_firmware_slot(uint8_t slot) {
  uint32_t slot_start;
  
  if (slot == 0U) {
    slot_start = FLASH_REGION_FIRMWARE_SLOT_0_BEGIN;
  } else {
    slot_start = FLASH_REGION_FIRMWARE_SLOT_1_BEGIN;
  }

  pbl_flash_erase(FLASH, slot_start, SUBSECTOR_SIZE_BYTES);
}

#endif