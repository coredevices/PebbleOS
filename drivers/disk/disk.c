/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/disk.h>

PBL_DEVICE_CLASS_DEFINE(pbl_disk_class, "disk");

int pbl_disk_open(const struct pbl_disk *disk) {
  if (!pbl_device_is_ready(&disk->dev)) {
    return -ENODEV;
  }

  return disk->ops->open(disk);
}

int pbl_disk_read(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf) {
  return disk->ops->read(disk, sector, count, buf);
}

int pbl_disk_write(const struct pbl_disk *disk, uint32_t sector, uint32_t count, const void *buf) {
  return disk->ops->write(disk, sector, count, buf);
}

int pbl_disk_get_capacity(const struct pbl_disk *disk, uint32_t *count) {
  return disk->ops->get_capacity(disk, count);
}
