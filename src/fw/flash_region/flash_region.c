/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_region.h"

#include "drivers/flash.h"
#include "drivers/task_watchdog.h"
#include "kernel/util/sleep.h"
#include "system/logging.h"
#include "system/passert.h"

#include <inttypes.h>

//! Do some upkeep in between erases to keep the rest of the system stable since erases block
//! the current task for so long.
//!
//! @param erase_count[in, out] Counter variable to track how many times we've run this upkeep
//                              function
//! @param feed_watchdog Whether we should feed the task_watchdog for the current task or not
static void prv_erase_upkeep(int *erase_count, bool feed_watchdog) {
  (*erase_count)++;
  if ((*erase_count %= 2) == 0) {
    // Sleep if this is the second time in a row calling erase (sub)sector.

    // FIXME: We could check and see if we are starving tasks and only force a
    // context switch for that case
    psleep((SECTOR_SIZE_BYTES > (64 * 1024)) ? 20 : 4);
  }

  if (feed_watchdog) {
    task_watchdog_bit_set(pebble_task_get_current());
  }
}

static void prv_erase_optimal_range(uint32_t min_start, uint32_t max_start,
                                    uint32_t min_end, uint32_t max_end, bool feed_watchdog) {
  PBL_LOG(LOG_LEVEL_DEBUG, "flash_region_erase_optimal_range, 0x%"PRIx32" 0x%"PRIx32" 0x%"PRIx32" 0x%"PRIx32,
      min_start, max_start, min_end, max_end);

  PBL_ASSERTN(((min_start & (~SUBSECTOR_ADDR_MASK)) == 0) &&
              ((max_end & (~SUBSECTOR_ADDR_MASK)) == 0) &&
              (min_start <= max_start) &&
              (max_start <= min_end) &&
              (min_end <= max_end));

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
  prv_erase_upkeep(&erase_count, feed_watchdog);

  if (sector_start < sector_end) {
    // Now erase the leading subsectors...
    for (int32_t i = subsector_start; i < sector_start; i += SUBSECTOR_SIZE_BYTES) {
      flash_erase_subsector_blocking(i);
      prv_erase_upkeep(&erase_count, feed_watchdog);
    }

    // Erase the full sectors...
    for (int32_t i = sector_start; i < sector_end; i += SECTOR_SIZE_BYTES) {
      flash_erase_sector_blocking(i);
      prv_erase_upkeep(&erase_count, feed_watchdog);
    }

    // Erase the trailing subsectors
    for (int32_t i = sector_end; i < subsector_end; i += SUBSECTOR_SIZE_BYTES) {
      flash_erase_subsector_blocking(i);
      prv_erase_upkeep(&erase_count, feed_watchdog);
    }
  } else {
    // Can't erase any full sectors, just erase subsectors the whole way.
    for (int32_t i = subsector_start; i < subsector_end; i += SUBSECTOR_SIZE_BYTES) {
      flash_erase_subsector_blocking(i);
      prv_erase_upkeep(&erase_count, feed_watchdog);
    }
  }
}

void flash_region_erase_optimal_range(uint32_t min_start, uint32_t max_start,
                                      uint32_t min_end, uint32_t max_end) {
  prv_erase_optimal_range(min_start, max_start, min_end, max_end, false /* feed_watchdog */);
}

void flash_region_erase_optimal_range_no_watchdog(uint32_t min_start, uint32_t max_start,
                                                  uint32_t min_end, uint32_t max_end) {
  prv_erase_optimal_range(min_start, max_start, min_end, max_end, true /* feed_watchdog */);
}
