/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/disk.h>
#include <pbl/drivers/disk/ram.h>

#define SECTOR_SIZE  512U
#define SECTOR_COUNT (CONFIG_DISK_RAM_SIZE * 1024U / SECTOR_SIZE)

static uint8_t s_data[SECTOR_COUNT][SECTOR_SIZE];

static int prv_get_info(const struct pbl_disk *disk, struct pbl_disk_info *info) {
  *info = (struct pbl_disk_info){
    .sector_size = SECTOR_SIZE,
    .sector_count = SECTOR_COUNT,
  };

  return 0;
}

static int prv_read(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf) {
  memcpy(buf, s_data[sector], count * SECTOR_SIZE);

  return 0;
}

static int prv_write(const struct pbl_disk *disk, uint32_t sector, uint32_t count,
                     const void *buf) {
  memcpy(s_data[sector], buf, count * SECTOR_SIZE);

  return 0;
}

static const struct pbl_disk_ops s_ops = {
  .get_info = prv_get_info,
  .read = prv_read,
  .write = prv_write,
};

PBL_DISK_STATE_DEFINE(pbl_disk_ram);
const struct pbl_disk pbl_disk_ram = PBL_DISK_INIT(pbl_disk_ram, "ramdisk", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_disk_ram, &pbl_disk_ram.dev);
