/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

//! Flash Translation Layer
//!
//! This module allows our filesystem, PFS, to grow into multiple flash regions while keeping a
//! contiguous virtual address space.
//!
//! On boot, this module checks each region to see if the filesystem is active in said region.
//! If so, it adds the region to the flash translation space and continues processing the
//! remaining regions. If the filesystem was not previously active in the region, then the region
//! is first migrated (by calling the migration function pointer) and is added to the
//! flash translation space.

//! Adds a flash region to the flash translation layer. This increases the overall size of the
//! flash translation space by (region_end - region_start)
//! @param region_start start of the region
//! @param region_end end of the region
//! @param erase_new_region Whether or not to erase the region before adding
void ftl_add_region(uint32_t region_start, uint32_t region_end, bool erase_new_region);

//! Gets the size of the flash translation space.
//! @return - the size of the flash translation space in number of bytes.
uint32_t ftl_get_size(void);

//! Erases a SECTOR in the flash translation space starting at the given virtual flash offset.
//! There is an ASSERT to check if size is exactly the size of the region being erased.
//!
//! @param size size of sector to erase. Should be equal to SUBSECTOR_SIZE_BYTES
//! @param offset virtual flash offset to erase the SUBSECTOR
void ftl_erase_sector(uint32_t size, uint32_t offset);
//! same as ftl_erase_sector except it operates on a SUBSECTOR
void ftl_erase_subsector(uint32_t size, uint32_t offset);

//! Reads the data at the virtual flash address given and writes it to the data buffer.
//! @param buffer The data block to write to
//! @param size The number of bytes from flash to write into buffer (must be <= the size of buffer)
//! @param offset Where to read the bytes from in the virtual flash translation space.
void ftl_read(void *buffer, size_t size, uint32_t offset);

//! Writes the data buffer to the virtual flash address given.
//! @param buffer The data block to write to flash
//! @param size The number of bytes from buffer to write (must be <= the size of buffer)
//! @param offset Where to write the bytes in the virtual flash translation space.
void ftl_write(const void *buffer, size_t size, uint32_t offset);

//! Formats all regions added to the flash translation layer
void ftl_format(void);

//! There are two steps to this function.
//!   1. Add all regions where PFS already exists, and add them to the flash translation layer.
//!   2. Migrate all regions where PFS does NOT exist and add them to the flash translation layer.
void ftl_populate_region_list(void);

void add_initial_space_to_filesystem(void);
