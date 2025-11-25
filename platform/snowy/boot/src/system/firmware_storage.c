/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "firmware_storage.h"

#include "drivers/flash.h"

FirmwareDescription firmware_storage_read_firmware_description(uint32_t firmware_start_address) {
  FirmwareDescription firmware_description;
  flash_read_bytes((uint8_t*) &firmware_description, firmware_start_address, sizeof(FirmwareDescription));
  return firmware_description;
}

bool firmware_storage_check_valid_firmware_description(FirmwareDescription* desc) {
  return desc->description_length == sizeof(FirmwareDescription);
}
