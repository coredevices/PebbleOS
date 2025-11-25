/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file firmware_storage.h
//! Utilities for reading a firmware image stored in flash.

#include <stdbool.h>
#include <stdint.h>

typedef struct __attribute__((__packed__)) FirmwareDescription {
  uint32_t description_length;
  uint32_t firmware_length;
  uint32_t checksum;
} FirmwareDescription;

FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address);

bool firmware_storage_check_valid_firmware_description(FirmwareDescription* desc);
