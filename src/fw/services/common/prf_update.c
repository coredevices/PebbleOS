/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "system/bootbits.h"
#include "system/firmware_storage.h"
#include "system/logging.h"
#include "util/math.h"

// Don't allow PRF updating when we're in PRF
#ifndef RECOVERY_FW
static void prv_do_update(void) {
  PBL_LOG(LOG_LEVEL_INFO, "Updating PRF!");
  flash_prf_set_protection(false);

  bool saved_sleep_when_idle = flash_get_sleep_when_idle();
  flash_sleep_when_idle(false);

#if !CAPABILITY_HAS_PBLBOOT
  FirmwareDescription description =
      firmware_storage_read_firmware_description(FLASH_REGION_FIRMWARE_DEST_BEGIN);

  if (!firmware_storage_check_valid_firmware_description(FLASH_REGION_FIRMWARE_DEST_BEGIN,
                                                         &description)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Invalid recovery firmware CRC in SPI flash!");
    goto done;
  }

  const uint32_t total_length = description.description_length + description.firmware_length;
#else
  FirmwareHeader header =
      firmware_storage_read_firmware_header(FLASH_REGION_FIRMWARE_DEST_BEGIN);
  if (!firmware_storage_check_valid_firmware_header(FLASH_REGION_FIRMWARE_DEST_BEGIN,
                                                    &header)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Invalid recovery firmware CRC in SPI flash!");
    goto done;
  }

  const uint32_t total_length = header.fw_start + header.fw_length;
#endif

  PBL_LOG(LOG_LEVEL_DEBUG, "Erasing previous PRF...");
  flash_region_erase_optimal_range(FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                                   FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                                   FLASH_REGION_SAFE_FIRMWARE_BEGIN + total_length,
                                   FLASH_REGION_SAFE_FIRMWARE_END);

  PBL_LOG(LOG_LEVEL_DEBUG, "Copying PRF from scratch to the PRF slot");
  uint8_t buffer[512];
  uint32_t offset = 0;
  while (offset < total_length) {
    const uint32_t chunk_size = MIN(sizeof(buffer), (total_length - offset));

    flash_read_bytes(buffer, FLASH_REGION_FIRMWARE_DEST_BEGIN + offset, chunk_size);
    flash_write_bytes(buffer, FLASH_REGION_SAFE_FIRMWARE_BEGIN + offset, chunk_size);

    offset += chunk_size;
  }

done:
  flash_prf_set_protection(true);
  flash_sleep_when_idle(saved_sleep_when_idle);
  PBL_LOG(LOG_LEVEL_DEBUG, "Done!");
}
#endif

void check_prf_update(void) {
  if (!boot_bit_test(BOOT_BIT_NEW_PRF_AVAILABLE)) {
    return;
  }

  boot_bit_clear(BOOT_BIT_NEW_PRF_AVAILABLE);

#ifndef RECOVERY_FW
  prv_do_update();
#endif
}
