/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_disk Disk
 * @ingroup drivers
 * @brief Disk class: storage read and written in whole sectors.
 *
 * A disk holds a medium, which may come and go, as a memory card: pbl_disk_open() brings it up,
 * and fails with -ENOMEDIUM without one. Its sectors are then read and written whole, by number.
 *
 * @code{.c}
 * uint8_t sector[512];
 * uint32_t count;
 *
 * if (pbl_disk_open(disk) == 0 && pbl_disk_get_capacity(disk, &count) == 0) {
 *   ret = pbl_disk_read(disk, 0U, 1U, sector);
 * }
 * @endcode
 * @{
 */

struct pbl_disk;

/** @brief Class of the disks. */
extern const struct pbl_device_class pbl_disk_class;

/** @brief Disk operations, implemented by the drivers. */
struct pbl_disk_ops {
  /** Bring the medium up, if it is not, or -ENOMEDIUM without one. */
  int (*open)(const struct pbl_disk *disk);
  /** Read whole sectors. */
  int (*read)(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf);
  /** Write whole sectors. */
  int (*write)(const struct pbl_disk *disk, uint32_t sector, uint32_t count, const void *buf);
  /** Number of sectors of the medium. */
  int (*get_capacity)(const struct pbl_disk *disk, uint32_t *count);
};

/** @brief Disk. */
struct pbl_disk {
  /** Device. */
  struct pbl_device dev;
  /** Operations. */
  const struct pbl_disk_ops *ops;
  /** Sector size in bytes. */
  uint32_t sector_size;
};

/**
 * @brief Initializer of a struct pbl_disk, for drivers.
 *
 * @param sym Disk symbol, with its PBL_DEVICE_STATE_DEFINE().
 * @param _name Device name.
 * @param _init Device init function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Operations.
 * @param _sector_size Sector size in bytes.
 */
#define PBL_DISK_INIT(sym, _name, _init, _parent, _ops, _sector_size)          \
  {                                                                            \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, &pbl_disk_class), \
    .ops = (_ops),                                                             \
    .sector_size = (_sector_size),                                             \
  }

/**
 * @brief Bring the medium up.
 *
 * @param disk Disk.
 * @return 0, -ENODEV if the disk is not ready, -ENOMEDIUM without a medium, or a negative errno.
 */
int pbl_disk_open(const struct pbl_disk *disk);

/**
 * @brief Read whole sectors.
 *
 * @param disk Disk, opened.
 * @param sector First sector.
 * @param count Number of sectors.
 * @param buf Buffer of @p count sectors.
 * @return 0, -EINVAL beyond the medium, or a negative errno.
 */
int pbl_disk_read(const struct pbl_disk *disk, uint32_t sector, uint32_t count, void *buf);

/**
 * @brief Write whole sectors.
 *
 * @param disk Disk, opened.
 * @param sector First sector.
 * @param count Number of sectors.
 * @param buf Data of @p count sectors.
 * @return 0, -EINVAL beyond the medium, or a negative errno.
 */
int pbl_disk_write(const struct pbl_disk *disk, uint32_t sector, uint32_t count, const void *buf);

/**
 * @brief Number of sectors of the medium.
 *
 * @param disk Disk, opened.
 * @param count Number of sectors.
 * @return 0, or a negative errno.
 */
int pbl_disk_get_capacity(const struct pbl_disk *disk, uint32_t *count);

/** @} */
