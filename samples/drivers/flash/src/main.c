/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/crc/crc.h>
#include <pbl/drivers/flash.h>
#include <pbl/kernel/time.h>
#include <pbl/util/math.h>

#define CHUNK     256U
#define CHECK_LEN (64U * 1024U)

static uint8_t s_buf[CHUNK];

/*
 * Only reads: the flash holds the board's firmware and data, and has no area a sample may
 * erase. Writing takes an erased range: pbl_flash_erase() first, then pbl_flash_write().
 */
int main(void) {
  const struct pbl_flash_geometry *geo = pbl_flash_get_geometry(FLASH);
  uint32_t len = PBL_MIN(CHECK_LEN, geo->size);
  uint32_t crc = 0U;
  uint64_t start;
  int ret;

  printf("flash: %d KiB, pages of %d bytes\n", (int)(geo->size / 1024U), (int)geo->page_size);
  printf("flash: erases %d KiB sectors and %d KiB subsectors\n", (int)(geo->sector_size / 1024U),
         (int)(geo->subsector_size / 1024U));

  /* Addresses are offsets from the first byte of the flash. */
  start = pbl_uptime_ms();
  for (uint32_t addr = 0U; addr < len; addr += CHUNK) {
    ret = pbl_flash_read(FLASH, addr, s_buf, CHUNK);
    if (ret != 0) {
      fprintf(stderr, "flash: read at %d failed (%d)\n", (int)addr, ret);
      return ret;
    }

    crc = pbl_crc32(crc, s_buf, CHUNK);
  }
  printf("flash: read %d KiB in %d ms\n", (int)(len / 1024U), (int)(pbl_uptime_ms() - start));

  printf("flash: CRC-32 0x%08x\n", (unsigned int)crc);

  /* The class can checksum a range itself, without a buffer of the caller. */
  printf("flash: the same as the class computes: %s\n",
         crc == pbl_flash_crc32(FLASH, 0U, len) ? "yes" : "no");

  printf("flash: last subsector erased: %s\n",
         pbl_flash_is_erased(FLASH, geo->size - geo->subsector_size, geo->subsector_size) ? "yes"
                                                                                          : "no");

  printf("Sample done\n");

  return 0;
}
