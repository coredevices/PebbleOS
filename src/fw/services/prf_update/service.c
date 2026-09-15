/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>
#include "flash_region/flash_region.h"
#include "system/bootbits.h"
#include "system/firmware_storage.h"
#include <pbl/logging/logging.h>
#include "pbl/util/math.h"

PBL_LOG_MODULE_DEFINE(service_prf_update, CONFIG_SERVICE_PRF_UPDATE_LOG_LEVEL);

// Don't allow PRF updating when we're in PRF
#ifndef CONFIG_RECOVERY_FW
static void prv_do_update(void) {
  PBL_LOG_INFO("Updating PRF!");
  pbl_flash_unprotect(FLASH);

#ifndef CONFIG_PBLBOOT
  FirmwareDescription description =
      firmware_storage_read_firmware_description(FLASH_REGION_FIRMWARE_DEST_BEGIN);

  if (!firmware_storage_check_valid_firmware_description(FLASH_REGION_FIRMWARE_DEST_BEGIN,
                                                         &description)) {
    PBL_LOG_WRN("Invalid recovery firmware CRC in SPI flash!");
    goto done;
  }

  const uint32_t total_length = description.description_length + description.firmware_length;
#else
  FirmwareHeader header =
      firmware_storage_read_firmware_header(FLASH_REGION_FIRMWARE_DEST_BEGIN);
  if (!firmware_storage_check_valid_firmware_header(FLASH_REGION_FIRMWARE_DEST_BEGIN,
                                                    &header)) {
    PBL_LOG_WRN("Invalid recovery firmware CRC in SPI flash!");
    goto done;
  }

  const uint32_t total_length = header.fw_start + header.fw_length;
#endif

  PBL_LOG_DBG("Erasing previous PRF...");
  pbl_flash_erase(FLASH, FLASH_REGION_SAFE_FIRMWARE_BEGIN, total_length);

  PBL_LOG_DBG("Copying PRF from scratch to the PRF slot");
  uint8_t buffer[512];
  uint32_t offset = 0;
  while (offset < total_length) {
    const uint32_t chunk_size = MIN(sizeof(buffer), (total_length - offset));

    pbl_flash_read(FLASH, FLASH_REGION_FIRMWARE_DEST_BEGIN + offset, buffer, chunk_size);
    pbl_flash_write(FLASH, FLASH_REGION_SAFE_FIRMWARE_BEGIN + offset, buffer, chunk_size);

    offset += chunk_size;
  }

done:
  pbl_flash_protect(FLASH, FLASH_REGION_SAFE_FIRMWARE_BEGIN,
                    FLASH_REGION_SAFE_FIRMWARE_END - FLASH_REGION_SAFE_FIRMWARE_BEGIN);
  PBL_LOG_DBG("Done!");
}
#endif

void check_prf_update(void) {
  if (!boot_bit_test(BOOT_BIT_NEW_PRF_AVAILABLE)) {
    return;
  }

  boot_bit_clear(BOOT_BIT_NEW_PRF_AVAILABLE);

#ifndef CONFIG_RECOVERY_FW
  prv_do_update();
#endif
}
