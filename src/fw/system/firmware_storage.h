/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file firmware_storage.h
//! Utilities for reading a firmware image stored in flash.

#include "util/attributes.h"

#include <stdbool.h>
#include <stdint.h>

#if !CAPABILITY_HAS_PBLBOOT
typedef struct PACKED FirmwareDescription {
  uint32_t description_length;
  uint32_t firmware_length;
  uint32_t checksum;
} FirmwareDescription;

FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address);

bool firmware_storage_check_valid_firmware_description(
    uint32_t firmware_start_address, const FirmwareDescription* firmware_description);
#else

#define FIRMWARE_HEADER_MAGIC 0x96f3b83d

typedef struct PACKED FirmwareHeader {
  uint32_t magic;
  uint32_t header_length;
  uint64_t fw_timestamp;
  uint32_t fw_start;
  uint32_t fw_length;
  uint32_t fw_crc;
} FirmwareHeader;

FirmwareHeader firmware_storage_read_firmware_header(uint32_t address);
bool firmware_storage_check_valid_firmware_header(
    uint32_t address, const FirmwareHeader* header);

void firmware_storage_invalidate_firmware_slot(uint8_t slot);
#endif