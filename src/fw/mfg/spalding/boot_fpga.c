/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg/spalding/mfg_private.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "mfg/spalding/spalding_boot.fpga.auto.h"
#include "system/logging.h"
#include "util/attributes.h"
#include "util/crc32.h"

#include <inttypes.h>

const uintptr_t BOOT_FPGA_FLASH_ADDR = FLASH_REGION_MFG_INFO_BEGIN + 0x10000;

typedef struct PACKED {
  uint16_t fpga_len;
  uint16_t fpga_len_complemented;
} BootFPGAHeader;

bool mfg_info_is_boot_fpga_bitstream_written(void) {
  BootFPGAHeader expected_fpga_header = { (uint16_t)sizeof s_boot_fpga,
                                          (uint16_t)~sizeof s_boot_fpga };

  BootFPGAHeader header;
  flash_read_bytes((void *)&header, BOOT_FPGA_FLASH_ADDR, sizeof header);
  if (header.fpga_len != expected_fpga_header.fpga_len ||
      header.fpga_len_complemented != expected_fpga_header.fpga_len_complemented) {

    PBL_LOG(LOG_LEVEL_DEBUG, "Boot FPGA length invalid, needs a rewrite");

    // The length doesn't even match, we definitely need to update.
    return false;
  }

  // Just because the length is the same that doesn't mean we don't need to update the FPGA
  // image. Compare CRCs to see if the new FPGA image is different.
  uint32_t expected_crc = crc32(CRC32_INIT, s_boot_fpga, sizeof(s_boot_fpga));
  uint32_t stored_crc = flash_crc32(
      BOOT_FPGA_FLASH_ADDR + sizeof(BootFPGAHeader), sizeof(s_boot_fpga));

  PBL_LOG(LOG_LEVEL_DEBUG, "Comparing boot FPGA CRCs, expected 0x%"PRIx32" found 0x%"PRIx32,
          expected_crc, stored_crc);

  return expected_crc == stored_crc;
}

void mfg_info_write_boot_fpga_bitstream(void) {
  // Store the bootloader FPGA in the MFG info flash region so that the
  // bootloader can find it.
  _Static_assert(sizeof s_boot_fpga < 1<<16, "FPGA bitstream too big");
  BootFPGAHeader fpga_header = { (uint16_t)sizeof s_boot_fpga,
                                 (uint16_t)~sizeof s_boot_fpga };

  // I have no idea why but clang really hates this assert
  //
  // ../../src/fw/mfg/spalding/boot_fpga.c:50:7: error: static_assert expression is not an
  // integral constant expression
  //       (BOOT_FPGA_FLASH_ADDR + sizeof(BootFPGAHeader) + sizeof(s_boot_fpga))
  //       ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#if !__clang__
  _Static_assert(
      (BOOT_FPGA_FLASH_ADDR + sizeof(BootFPGAHeader) + sizeof(s_boot_fpga))
      < FLASH_REGION_MFG_INFO_END,
      "FPGA bitstream will overflow FLASH_REGION_MFG_INFO!");
#endif

  flash_write_bytes((const uint8_t *)&fpga_header,
                    BOOT_FPGA_FLASH_ADDR, sizeof fpga_header);
  flash_write_bytes((const uint8_t *)s_boot_fpga,
                    BOOT_FPGA_FLASH_ADDR + sizeof fpga_header,
                    sizeof s_boot_fpga);
}
