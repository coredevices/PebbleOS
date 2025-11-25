/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_translation.h"

#include "drivers/flash.h"
#include "drivers/task_watchdog.h"
#include "flash_region/filesystem_regions.h"
#include "flash_region/flash_region.h"
#include "services/normal/filesystem/pfs.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"
#include "util/size.h"

//! Flash translation operation
typedef enum {
  FTLRead,
  FTLWrite,
  FTLEraseSector,
  FTLEraseSubsector
} FTLOperation;

//! Total number of FSRegions listed in s_region_list
static const unsigned int TOTAL_NUM_FLASH_REGIONS = ARRAY_LENGTH(s_region_list);

//! Keeps track of the current total size of our filesystem in bytes.
static uint32_t s_ftl_size;

//! Keeps track of which regions are included in the filesystem.
static unsigned int s_next_region_idx = 0;

//! returns FSRegion size given idx in s_region_list
static uint32_t prv_region_size(int idx) {
  return (s_region_list[idx].end - s_region_list[idx].start);
}

//! add all regions temporarily so that PFS can test on these regions
static void prv_layout_version_add_all_regions(bool revert) {
  static unsigned int original_idx;

  if (!revert) {
    original_idx = s_next_region_idx;
    s_next_region_idx = TOTAL_NUM_FLASH_REGIONS;
  } else {
    s_next_region_idx = original_idx;
  }

  s_ftl_size = 0;

  for (unsigned int i = 0; i < s_next_region_idx; i++) {
    s_ftl_size += prv_region_size(i);
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Filesystem: Temporary size - %"PRId32" Kb", (s_ftl_size / 1024));
  pfs_set_size(s_ftl_size, false /* don't erase regions */);
}

//! return a layout version that associates with the labels from above
static uint8_t prv_ftl_get_layout_version(void) {
  // add all regions so PFS can know about them temporarily
  prv_layout_version_add_all_regions(false);

  uint8_t flash_version = 0;
  uint32_t known_size = 0;

  // iterate through all regions idx > 0 and see if PFS is active in the regions. If yes, increment
  // flash version.
  for (uint8_t i = flash_version; i < TOTAL_NUM_FLASH_REGIONS; i++) {
    if ((prv_region_size(i) == 0) ||
         pfs_active_in_region(known_size, known_size + prv_region_size(i))) {
      // if active, increment known flash version and increase size to check next region
      flash_version = i + 1;
      known_size += prv_region_size(i);
    } else {
      // if not active, break and return known flash version
      break;
    }
  }

  // go back to the state we were in before the function
  prv_layout_version_add_all_regions(true);

  return flash_version;
}

void ftl_add_region(uint32_t region_start, uint32_t region_end, bool erase_new_region) {
  // check if this region equals the next region, if so, then add next region
  if ((region_start == s_region_list[s_next_region_idx].start) &&
      (region_end == s_region_list[s_next_region_idx].end) &&
      (s_next_region_idx < TOTAL_NUM_FLASH_REGIONS)) {
    s_next_region_idx++;
  // failure, should never happen
  } else {
    PBL_LOG(LOG_LEVEL_WARNING,
        "Filesystem: Uh oh, we somehow added regions in the wrong order, %"PRIu32" %"PRIu32,
        region_start, region_end);
    return;
  }

  // erase if asked to
  if (erase_new_region) {
    flash_region_erase_optimal_range_no_watchdog(region_start, region_start,
                                                 region_end, region_end);
  }

  s_ftl_size += (region_end - region_start);

  // call back to PFS to make sure it realizes there is more space to place files.
  pfs_set_size(s_ftl_size, erase_new_region);
}

void ftl_populate_region_list(void) {
  uint8_t flash_layout_version = prv_ftl_get_layout_version();
  PBL_LOG(LOG_LEVEL_INFO, "Filesystem: Old Flash Layout Version: %u", flash_layout_version);

  for (unsigned int i = s_next_region_idx; i < flash_layout_version; i++) {
    ftl_add_region(s_region_list[i].start, s_region_list[i].end, false);
  }

  // at this point we have found all the regions that already exist on the flash
  // so run our cleanup logic in case we rebooted during a filesystem operation
  pfs_reboot_cleanup();

  for (unsigned int i = s_next_region_idx; i < TOTAL_NUM_FLASH_REGIONS; i++) {
    ftl_add_region(s_region_list[i].start, s_region_list[i].end, true);
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Filesystem: New size - %"PRId32" Kb", (s_ftl_size / 1024));
}

uint32_t ftl_get_size(void) {
  return s_ftl_size;
}

static void prv_ftl_operation(uint8_t *buffer, uint32_t size, uint32_t offset,
    FTLOperation operation) {

  uint32_t curr_virt_offset_begin = 0;
  uint32_t curr_virt_offset_end = 0;


  // iterate through all regions and perform read, write, or erase
  for (unsigned int idx = 0; (idx < s_next_region_idx) && (size != 0); idx++) {
    curr_virt_offset_end += prv_region_size(idx);
    if (offset < curr_virt_offset_end) {
      uint32_t bytes = MIN(curr_virt_offset_end - offset, size);

      if (operation == FTLRead) {
        flash_read_bytes(
            buffer, s_region_list[idx].start + offset - curr_virt_offset_begin, bytes);
      } else if (operation == FTLWrite ) {
        flash_write_bytes(
            buffer, s_region_list[idx].start + offset - curr_virt_offset_begin, bytes);
      } else if (operation == FTLEraseSubsector) {
        PBL_ASSERTN(size == SUBSECTOR_SIZE_BYTES);
        flash_erase_subsector_blocking(
            s_region_list[idx].start + offset - curr_virt_offset_begin);
      } else if (operation == FTLEraseSector) {
        PBL_ASSERTN(size == SECTOR_SIZE_BYTES);
        flash_erase_sector_blocking(
            s_region_list[idx].start + offset - curr_virt_offset_begin);
      }

      size -= bytes;
      offset += bytes;
    }
    curr_virt_offset_begin = curr_virt_offset_end;
  }
}

void ftl_read(void *buffer, size_t size, uint32_t offset) {
  prv_ftl_operation(buffer, size, offset, FTLRead);
}

void ftl_write(const void *buffer, size_t size, uint32_t offset) {
  prv_ftl_operation((void *)buffer, size, offset, FTLWrite);
}

void ftl_erase_sector(uint32_t size, uint32_t offset) {
  prv_ftl_operation(NULL /* not needed for erase */, size, offset, FTLEraseSector);
}

void ftl_erase_subsector(uint32_t size, uint32_t offset) {
  prv_ftl_operation(NULL /* not needed for erase */, size, offset, FTLEraseSubsector);
}

void ftl_format(void) {
}

//! Only used for tests.
void ftl_force_version(int version_idx) {
  s_next_region_idx = version_idx;
  s_ftl_size = 0;
  for (int i = 0; i < version_idx; i++) {
    s_ftl_size += prv_region_size(i);
  }

  pfs_set_size(s_ftl_size, false);
  extern void test_force_recalc_of_gc_region(void);
  test_force_recalc_of_gc_region();
}
