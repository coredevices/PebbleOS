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

#include "firmware_storage.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "system/logging.h"
#include "util/math.h"

#if !CAPABILITY_HAS_PBLBOOT
FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address) {
  FirmwareDescription firmware_description;
  flash_read_bytes((uint8_t*) &firmware_description, firmware_start_address,
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
  PBL_LOG(LOG_LEVEL_DEBUG, "CRCing recovery...");

  start_address += sizeof(FirmwareDescription);
#if CAPABILITY_HAS_DEFECTIVE_FW_CRC
  const uint32_t calculated_crc = flash_calculate_legacy_defective_checksum(
      start_address, firmware_description->firmware_length);
#else
  const uint32_t calculated_crc = flash_crc32(start_address, firmware_description->firmware_length);
#endif

  PBL_LOG(LOG_LEVEL_DEBUG, "CRCing recovery... done");

  return calculated_crc == firmware_description->checksum;
}
#else
FirmwareHeader firmware_storage_read_firmware_header(uint32_t address) {
  FirmwareHeader header;
  flash_read_bytes((uint8_t*) &header, address, sizeof(FirmwareHeader));
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
  PBL_LOG(LOG_LEVEL_DEBUG, "CRCing recovery...");

  const uint32_t calculated_crc = flash_crc32(address + header->fw_start, header->fw_length);

  PBL_LOG(LOG_LEVEL_DEBUG, "CRCing recovery... done");

  return calculated_crc == header->fw_crc;
}

void firmware_storage_invalidate_firmware_slot(uint8_t slot) {
  uint32_t slot_start;
  
  if (slot == 0U) {
    slot_start = FLASH_REGION_FIRMWARE_SLOT_0_BEGIN;
  } else {
    slot_start = FLASH_REGION_FIRMWARE_SLOT_1_BEGIN;
  }

  FirmwareHeader hdr = firmware_storage_read_firmware_header(slot_start);
  flash_region_erase_optimal_range(slot_start,
                                   slot_start,
                                   ROUND_TO_MOD_CEIL((slot_start + hdr.fw_start), SUBSECTOR_SIZE_BYTES),
                                   ROUND_TO_MOD_CEIL((slot_start + hdr.fw_start), SUBSECTOR_SIZE_BYTES));
}

#endif