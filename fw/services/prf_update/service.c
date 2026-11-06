/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

#include <system/bootbits.h>
#include <system/firmware_storage.h>

PBL_LOG_MODULE_DEFINE(service_prf_update, CONFIG_SERVICE_PRF_UPDATE_LOG_LEVEL);

// Don't allow PRF updating when we're in PRF
#ifndef CONFIG_RECOVERY_FW
static void prv_do_update(void) {
  PBL_LOG_INFO("Updating PRF!");
  (void)pbl_flash_unprotect(FLASH);

#ifndef CONFIG_PBLBOOT
  FirmwareDescription description =
      firmware_storage_read_firmware_description(PART_FIRMWARE_DEST_OFFSET);

  if (!firmware_storage_check_valid_firmware_description(PART_FIRMWARE_DEST_OFFSET, &description)) {
    PBL_LOG_WRN("Invalid recovery firmware CRC in SPI flash!");
    goto done;
  }

  const uint32_t total_length = description.description_length + description.firmware_length;
#else
  FirmwareHeader header = firmware_storage_read_firmware_header(PART_FIRMWARE_DEST_OFFSET);
  if (!firmware_storage_check_valid_firmware_header(PART_FIRMWARE_DEST_OFFSET, &header)) {
    PBL_LOG_WRN("Invalid recovery firmware CRC in SPI flash!");
    goto done;
  }

  const uint32_t total_length = header.fw_start + header.fw_length;
#endif

  if (total_length > PART_SAFE_FIRMWARE_SIZE) {
    PBL_LOG_WRN("Recovery firmware does not fit the PRF slot!");
    goto done;
  }

  PBL_LOG_DBG("Erasing previous PRF...");
  pbl_flash_partition_erase(PART_SAFE_FIRMWARE, 0U, total_length);

  PBL_LOG_DBG("Copying PRF from scratch to the PRF slot");
  uint8_t buffer[512];
  uint32_t offset = 0;
  while (offset < total_length) {
    const uint32_t chunk_size = MIN(sizeof(buffer), (total_length - offset));

    pbl_flash_partition_read(PART_FIRMWARE_DEST, offset, buffer, chunk_size);
    pbl_flash_partition_write(PART_SAFE_FIRMWARE, offset, buffer, chunk_size);

    offset += chunk_size;
  }

done:
  (void)pbl_flash_partition_protect(PART_SAFE_FIRMWARE);
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
