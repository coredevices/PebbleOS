/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/disk.h>

/**
 * @defgroup sd SD cards
 * @ingroup subsys
 * @brief The SD card in the slot of the board's SD host controller, as a disk.
 *
 * The card stack brings the card up when the disk is opened, then moves its 512-byte blocks:
 *
 * @code{.c}
 * uint32_t count;
 *
 * if (pbl_disk_open(SD_DISK) == 0 && pbl_disk_get_capacity(SD_DISK, &count) == 0) {
 *   ret = pbl_disk_read(SD_DISK, 0U, 1U, sector);
 * }
 * @endcode
 * @{
 */

/** @brief The SD card, on the board's SD host controller, its @c sdhc role. */
extern const struct pbl_disk *const SD_DISK;

/** @} */
