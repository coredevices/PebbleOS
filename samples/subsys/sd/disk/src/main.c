/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/cache.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/time.h>
#include <pbl/sd/sd.h>

#define SECTOR_SIZE   512U
#define SPEED_SECTORS 2048U
#define CHUNK_SECTORS 8U

/* Aligned to the cache lines, for the controllers to read into them by DMA. */
static uint8_t s_saved[SECTOR_SIZE] PBL_ALIGNED(PBL_DCACHE_LINE_SIZE);
static uint8_t s_buf[CHUNK_SECTORS * SECTOR_SIZE] PBL_ALIGNED(PBL_DCACHE_LINE_SIZE);

int main(void) {
  uint32_t count;
  uint64_t start;
  bool same;
  int ret;

  /* The card comes up when the disk is opened, if there is one in the slot. */
  ret = pbl_disk_open(SD_DISK);
  if (ret == -ENOMEDIUM) {
    printf("sd: no card in the slot\n");
    return 0;
  }
  if (ret == 0) {
    ret = pbl_disk_get_capacity(SD_DISK, &count);
  }
  if (ret != 0) {
    fprintf(stderr, "sd: card failed (%d)\n", ret);
    return ret;
  }
  printf("sd: %lu MiB, %lu sectors\n", (unsigned long)(count / 2048U), (unsigned long)count);

  ret = pbl_disk_read(SD_DISK, 0U, 1U, s_buf);
  if (ret != 0) {
    fprintf(stderr, "sd: read failed (%d)\n", ret);
    return ret;
  }
  printf("sd: sector 0 %s a boot signature\n",
         s_buf[510] == 0x55U && s_buf[511] == 0xaaU ? "has" : "has no");

  /* The last sector, usually unused, is written with a pattern, read back and put back. */
  ret = pbl_disk_read(SD_DISK, count - 1U, 1U, s_saved);
  for (unsigned int i = 0U; ret == 0 && i < SECTOR_SIZE; i++) {
    s_buf[i] = (uint8_t)(i * 13U + 7U);
  }
  if (ret == 0) {
    ret = pbl_disk_write(SD_DISK, count - 1U, 1U, s_buf);
  }
  if (ret == 0) {
    memset(s_buf + SECTOR_SIZE, 0, SECTOR_SIZE);
    ret = pbl_disk_read(SD_DISK, count - 1U, 1U, s_buf + SECTOR_SIZE);
  }
  same = ret == 0 && memcmp(s_buf, s_buf + SECTOR_SIZE, SECTOR_SIZE) == 0;
  if (ret == 0) {
    ret = pbl_disk_write(SD_DISK, count - 1U, 1U, s_saved);
  }
  if (ret != 0) {
    fprintf(stderr, "sd: last sector failed (%d)\n", ret);
    return ret;
  }
  printf("sd: last sector written and read back: %s\n", same ? "yes" : "no");

  start = pbl_uptime_ms();
  for (uint32_t s = 0U; ret == 0 && s < SPEED_SECTORS; s += CHUNK_SECTORS) {
    ret = pbl_disk_read(SD_DISK, s, CHUNK_SECTORS, s_buf);
  }
  if (ret != 0) {
    fprintf(stderr, "sd: read failed (%d)\n", ret);
    return ret;
  }
  printf("sd: read 1 MiB in %lu ms\n", (unsigned long)(pbl_uptime_ms() - start));

  printf("Sample done\n");

  return 0;
}
