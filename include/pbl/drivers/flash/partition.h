/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/flash.h>

/**
 * @defgroup drivers_flash_partition Flash partitions
 * @ingroup drivers_flash
 * @brief Access to areas of the board's flash.
 *
 * Offsets given to these functions start at the partition, which they check them against. Code
 * working on flash addresses uses @c offset and @c size directly. The partitions themselves belong
 * to the board: see pbl/board/partitions.h.
 *
 * @code{.c}
 * static_assert(sizeof(struct state) <= PART_TZINFO_SIZE);
 *
 * ret = pbl_flash_partition_erase(PART_TZINFO, 0U, PART_TZINFO_SIZE);
 * if (ret != 0) {
 *   return ret;
 * }
 *
 * ret = pbl_flash_partition_write(PART_TZINFO, 0U, &state, sizeof(state));
 * @endcode
 * @{
 */

/** @brief An area of the board's flash. */
struct pbl_flash_partition {
  /** Offset of the first byte in the flash. */
  uint32_t offset;
  /** Size in bytes. */
  uint32_t size;
};

/**
 * @brief The partition described by the constants @p name_OFFSET and @p name_SIZE.
 *
 * @param name Prefix of the constants.
 */
#define PBL_FLASH_PARTITION(name) \
  ((struct pbl_flash_partition){.offset = name##_OFFSET, .size = name##_SIZE})

static inline bool pbl_flash_partition__in_range(struct pbl_flash_partition part, uint32_t offs,
                                                 size_t len) {
  return len <= part.size && offs <= (part.size - len);
}

/**
 * @brief Read data.
 *
 * @param part Partition.
 * @param offs Offset in the partition.
 * @param buf Buffer.
 * @param len Number of bytes.
 * @retval 0 On success.
 * @retval -EINVAL If the range is outside the partition.
 * @return Otherwise, the error of pbl_flash_read().
 */
static inline int pbl_flash_partition_read(struct pbl_flash_partition part, uint32_t offs,
                                           void *buf, size_t len) {
  if (!pbl_flash_partition__in_range(part, offs, len)) {
    return -EINVAL;
  }

  return pbl_flash_read(FLASH, part.offset + offs, buf, len);
}

/**
 * @brief Write data.
 *
 * @param part Partition.
 * @param offs Offset in the partition.
 * @param buf Data.
 * @param len Number of bytes.
 * @retval 0 On success.
 * @retval -EINVAL If the range is outside the partition.
 * @return Otherwise, the error of pbl_flash_write().
 */
static inline int pbl_flash_partition_write(struct pbl_flash_partition part, uint32_t offs,
                                            const void *buf, size_t len) {
  if (!pbl_flash_partition__in_range(part, offs, len)) {
    return -EINVAL;
  }

  return pbl_flash_write(FLASH, part.offset + offs, buf, len);
}

/**
 * @brief Erase a range.
 *
 * @param part Partition.
 * @param offs Offset in the partition, aligned to a subsector.
 * @param len Number of bytes.
 * @retval 0 On success.
 * @retval -EINVAL If the range is outside the partition.
 * @return Otherwise, the error of pbl_flash_erase().
 */
static inline int pbl_flash_partition_erase(struct pbl_flash_partition part, uint32_t offs,
                                            size_t len) {
  if (!pbl_flash_partition__in_range(part, offs, len)) {
    return -EINVAL;
  }

  return pbl_flash_erase(FLASH, part.offset + offs, len);
}

/**
 * @brief Check whether a range is erased.
 *
 * @param part Partition.
 * @param offs Offset in the partition.
 * @param len Number of bytes.
 * @return True if the range is inside the partition and erased.
 */
static inline bool pbl_flash_partition_is_erased(struct pbl_flash_partition part, uint32_t offs,
                                                 size_t len) {
  return pbl_flash_partition__in_range(part, offs, len) &&
         pbl_flash_is_erased(FLASH, part.offset + offs, len);
}

/**
 * @brief Protect a whole partition against writes and erases.
 *
 * @param part Partition.
 * @return The result of pbl_flash_protect().
 */
static inline int pbl_flash_partition_protect(struct pbl_flash_partition part) {
  return pbl_flash_protect(FLASH, part.offset, part.size);
}

/** @} */
