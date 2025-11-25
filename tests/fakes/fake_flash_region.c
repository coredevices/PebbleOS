/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash.h"
#include "flash_region/flash_region.h"

#include <stdint.h>

static void prv_erase_optimal_range(uint32_t min_start, uint32_t max_start,
                                    uint32_t min_end, uint32_t max_end) {
  // We want to erase the sector that starts immediately below max_start but after min_start. If no sector
  // boundary exists between the two, we need to start erasing sectors after min_start and backfill with
  // subsector erases.
  int32_t sector_start = (max_start & SECTOR_ADDR_MASK);
  int32_t subsector_start = (max_start & SUBSECTOR_ADDR_MASK);
  if (sector_start < (int32_t) min_start) {
    sector_start += SECTOR_SIZE_BYTES;
  }

  // We want to erase ending after min_end but before max_end. If that ends running past the end of max_end,
  // we need to erase starting with the sector before and fill in with subsector erases.
  int32_t sector_end = ((min_end - 1) & SECTOR_ADDR_MASK) + SECTOR_SIZE_BYTES;
  int32_t subsector_end = ((min_end - 1) & SUBSECTOR_ADDR_MASK) + SUBSECTOR_SIZE_BYTES;
  if (sector_end > (int32_t) max_end) {
    sector_end -= SECTOR_SIZE_BYTES;
  }

  int erase_count = 0;

  // Do the upkeep immediately just in case we've spent awhile running without feeding the
  // watchdog before doing this erase operation.

  if (sector_start < sector_end) {
    // Now erase the leading subsectors...
    for (int32_t i = subsector_start; i < sector_start; i += SUBSECTOR_SIZE_BYTES) {
      flash_erase_subsector_blocking(i);
    }

    // Erase the full sectors...
    for (int32_t i = sector_start; i < sector_end; i += SECTOR_SIZE_BYTES) {
      flash_erase_sector_blocking(i);
    }

    // Erase the trailing subsectors
    for (int32_t i = sector_end; i < subsector_end; i += SUBSECTOR_SIZE_BYTES) {
      flash_erase_subsector_blocking(i);
    }
  } else {
    // Can't erase any full sectors, just erase subsectors the whole way.
    for (int32_t i = subsector_start; i < subsector_end; i += SUBSECTOR_SIZE_BYTES) {
      flash_erase_subsector_blocking(i);
    }
  }
}

void flash_region_erase_optimal_range(uint32_t min_start, uint32_t max_start,
                                      uint32_t min_end, uint32_t max_end) {
  prv_erase_optimal_range(min_start, max_start, min_end, max_end);
}

void flash_region_erase_optimal_range_no_watchdog(uint32_t min_start, uint32_t max_start,
                                                  uint32_t min_end, uint32_t max_end) {
  prv_erase_optimal_range(min_start, max_start, min_end, max_end);
}
