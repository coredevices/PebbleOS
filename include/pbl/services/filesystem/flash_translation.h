/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @defgroup services_filesystem_flash_translation Flash translation layer
 * @ingroup services_filesystem
 * @brief Contiguous virtual address space for PFS over several flash regions.
 *
 * Lets PFS grow into multiple flash regions while keeping a contiguous virtual address space.
 * Regions are added in the order of the board's filesystem region list. On boot, the regions
 * where PFS is already active are added as they are; the remaining ones are erased and then
 * added, growing the filesystem.
 * @{
 */

/**
 * @brief Append a flash region to the translation space.
 *
 * The region must be the next one in the filesystem region list; otherwise the call is ignored.
 * Grows the PFS size by @p region_end - @p region_start.
 *
 * @param region_start Start address of the region.
 * @param region_end End address of the region, exclusive.
 * @param erase_new_region Erase the region before adding it.
 */
void ftl_add_region(uint32_t region_start, uint32_t region_end, bool erase_new_region);

/**
 * @brief Get the size of the translation space.
 *
 * @return Size in bytes.
 */
uint32_t ftl_get_size(void);

/**
 * @brief Erase a sector of the translation space.
 *
 * @param size Size of the area to erase, must equal @c SECTOR_SIZE_BYTES (asserted).
 * @param offset Virtual offset of the sector.
 */
void ftl_erase_sector(uint32_t size, uint32_t offset);

/**
 * @brief Erase a subsector of the translation space.
 *
 * @param size Size of the area to erase, must equal @c SUBSECTOR_SIZE_BYTES (asserted).
 * @param offset Virtual offset of the subsector.
 */
void ftl_erase_subsector(uint32_t size, uint32_t offset);

/**
 * @brief Read from the translation space.
 *
 * @param[out] buffer Destination buffer.
 * @param size Number of bytes to read, at most the size of @p buffer.
 * @param offset Virtual offset to read from.
 */
void ftl_read(void *buffer, size_t size, uint32_t offset);

/**
 * @brief Write to the translation space.
 *
 * @param buffer Data to write.
 * @param size Number of bytes to write, at most the size of @p buffer.
 * @param offset Virtual offset to write to.
 */
void ftl_write(const void *buffer, size_t size, uint32_t offset);

/**
 * @brief Format all regions of the translation space.
 *
 * Currently does nothing; use pfs_format() to erase the filesystem.
 */
void ftl_format(void);

/**
 * @brief Build the translation space at boot.
 *
 * Adds the regions where PFS already exists, runs pfs_reboot_cleanup(), then erases and adds
 * the remaining regions. Called by pfs_init().
 */
void ftl_populate_region_list(void);

/** @} */
