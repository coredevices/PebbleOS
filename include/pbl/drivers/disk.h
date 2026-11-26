/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_disk Disk
 * @ingroup drivers
 * @brief Block storage, read and written by whole sectors.
 *
 * A disk is an array of sectors of one size, numbered from 0. Reads and writes take whole sectors;
 * how the driver stores them, erase blocks and caching included, is its own. A write may stay in
 * the driver until pbl_disk_sync(). Calls are not serialized by the class.
 *
 * @code{.c}
 * struct pbl_disk_info info;
 *
 * ret = pbl_disk_get_info(disk, &info);
 * ret = pbl_disk_read(disk, 0U, 1U, buf);
 * @endcode
 * @{
 */

struct pbl_disk;

/** @brief Class of the disks. */
extern const struct pbl_device_class pbl_disk_class;

/** @brief What a disk is made of. */
struct pbl_disk_info {
  /** Size of a sector, in bytes. */
  uint32_t sector_size;
  /** Number of sectors. */
  uint32_t sector_count;
  /** Whether writes are refused. */
  bool read_only;
};

/** @brief Driver operations. */
struct pbl_disk_ops {
  /** Optional. Bring the disk up. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_disk *disk);
  /** Get what the disk is made of. Returns 0 or a negative errno. */
  int (*get_info)(const struct pbl_disk *disk, struct pbl_disk_info *info);
  /**
   * Read @p count sectors from @p sector, which the class has checked against the disk. Returns 0
   * or a negative errno.
   */
  int (*read)(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf);
  /**
   * Optional for a read-only disk. Write @p count sectors from @p sector, which the class has
   * checked against the disk. Returns 0 or a negative errno.
   */
  int (*write)(const struct pbl_disk *disk, uint32_t sector, uint32_t count, const void *buf);
  /** Optional. Store what writes left in the driver. Returns 0 or a negative errno. */
  int (*sync)(const struct pbl_disk *disk);
};

/** @brief Disk. */
struct pbl_disk {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_disk_ops *ops;
};

/** @cond INTERNAL_HIDDEN */
int pbl_disk_dev_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the runtime state of a disk.
 *
 * @param sym Symbol of the disk.
 */
#define PBL_DISK_STATE_DEFINE(sym) PBL_DEVICE_STATE_DEFINE(sym)

/**
 * @brief Initializer of a disk.
 *
 * @param sym Symbol of the disk.
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_DISK_INIT(sym, _name, _parent, _ops)                                           \
  {                                                                                        \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_disk_dev_init, _parent, &pbl_disk_class), \
    .ops = (_ops),                                                                         \
  }

/**
 * @brief Get what a disk is made of.
 *
 * @param disk Disk.
 * @param[out] info Sector size and count, and whether it is read-only.
 * @return 0, @c -ENODEV if the disk is not ready, or a negative errno from the driver.
 */
int pbl_disk_get_info(const struct pbl_disk *disk, struct pbl_disk_info *info);

/**
 * @brief Read sectors.
 *
 * @param disk Disk.
 * @param sector First sector.
 * @param count Number of sectors.
 * @param[out] buf Room for @p count sectors.
 * @return 0, @c -ENODEV if the disk is not ready, @c -EINVAL if the sectors are not all on the
 *         disk, or a negative errno from the driver.
 */
int pbl_disk_read(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf);

/**
 * @brief Write sectors.
 *
 * @param disk Disk.
 * @param sector First sector.
 * @param count Number of sectors.
 * @param buf @p count sectors.
 * @return 0, @c -ENODEV if the disk is not ready, @c -EINVAL if the sectors are not all on the
 *         disk, @c -EROFS if it is read-only, or a negative errno from the driver.
 */
int pbl_disk_write(const struct pbl_disk *disk, uint32_t sector, uint32_t count, const void *buf);

/**
 * @brief Store what writes left in the driver.
 *
 * @param disk Disk.
 * @return 0, @c -ENODEV if the disk is not ready, or a negative errno from the driver.
 */
int pbl_disk_sync(const struct pbl_disk *disk);

/** @} */
