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
#endif