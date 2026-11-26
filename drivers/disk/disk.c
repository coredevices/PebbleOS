/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/disk.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_disk_class, "disk");

int pbl_disk_dev_init(const struct pbl_device *dev) {
  const struct pbl_disk *disk = PBL_CONTAINER_OF(dev, const struct pbl_disk, dev);

  if (disk->ops->init == nullptr) {
    return 0;
  }

  return disk->ops->init(disk);
}

int pbl_disk_get_info(const struct pbl_disk *disk, struct pbl_disk_info *info) {
  if (!pbl_device_is_ready(&disk->dev)) {
    return -ENODEV;
  }

  return disk->ops->get_info(disk, info);
}

static int prv_check(const struct pbl_disk *disk, uint32_t sector, uint32_t count,
                     struct pbl_disk_info *info) {
  int ret;

  ret = pbl_disk_get_info(disk, info);
  if (ret != 0) {
    return ret;
  }

  if (sector > info->sector_count || count > info->sector_count - sector) {
    return -EINVAL;
  }

  return 0;
}

int pbl_disk_read(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf) {
  struct pbl_disk_info info;
  int ret;

  ret = prv_check(disk, sector, count, &info);
  if (ret != 0) {
    return ret;
  }

  if (count == 0U) {
    return 0;
  }

  return disk->ops->read(disk, sector, count, buf);
}

int pbl_disk_write(const struct pbl_disk *disk, uint32_t sector, uint32_t count, const void *buf) {
  struct pbl_disk_info info;
  int ret;

  ret = prv_check(disk, sector, count, &info);
  if (ret != 0) {
    return ret;
  }

  if (info.read_only || disk->ops->write == nullptr) {
    return -EROFS;
  }

  if (count == 0U) {
    return 0;
  }

  return disk->ops->write(disk, sector, count, buf);
}

int pbl_disk_sync(const struct pbl_disk *disk) {
  if (!pbl_device_is_ready(&disk->dev)) {
    return -ENODEV;
  }

  if (disk->ops->sync == nullptr) {
    return 0;
  }

  return disk->ops->sync(disk);
}
