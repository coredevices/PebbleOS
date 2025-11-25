/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "flash_region.h"

#include <stdint.h>

//! @file filesystem_regions.h
//!
//! This file describes the various regions that make up our filesystem. For historical reasons,
//! our filesystem is not one contiguous space in flash and is instead broken up across multiple
//! regions.

//! Individual filesystem region
typedef struct FSRegion {
  uint32_t start;
  uint32_t end;
} FSRegion;

// Note: Different platforms use different flash layouts (see flash_region/flash_region.h for more
// info).
//
// Our newer platforms only have one contiguous filesystem region which you can find below. Some
// legacy platforms (i.e Pebble OG and Pebble Steel) had flash regions added to the filesystem over
// time and are thus non-contiguous. For layouts with more than one region you will find their
// header included below.

#if PLATFORM_TINTIN
#include "filesystem_regions_n25q.h"
#else

// Typical single region filesystem layout
#define FILE_SYSTEM_REGIONS(MACRO_OPERATOR)                             \
  MACRO_OPERATOR(FLASH_REGION_FILESYSTEM_BEGIN, FLASH_REGION_FILESYSTEM_END)

#endif

// Notes:
//   In this file we check that individual region entries are sector aligned at the beginning and
//   the end because the filesystem only performs _sector_ erases.
//
//   To accomplish this at compile time we use a tiny bit of macro-fu. For each platform we expect
//   a 'FILE_SYSTEM_REGIONS(MACRO_OPERATOR)' definition. Within the macro definition, each
//   FSRegion's '.start' and '.end' is wrapped within a 'MACRO_OPERATOR'
//
//   The MACRO_OPERATOR argument is simply another macro that operates on the FLASH_SYSTEM_REGIONS
//   list.  This allows us to go through the list once to check layouts as a static assert (See
//   FILE_SYSTEM_LAYOUT_CHECK) and then go through the list to build the s_region_list struct (See
//   FILE_SYSTEM_FS_REGION_ENTRY_CONSTRUCTOR)
//
// Bonus Fun fact: 'analytics_metric_table.h' uses this same strategy to turn our analytics list
// into an enum, switch case statement, and AnalyticsMetricDataType array.

#define FILE_SYSTEM_LAYOUT_CHECK(s, e) \
  _Static_assert((s % SECTOR_SIZE_BYTES) == 0, "Filesystem region start not sector aligned"); \
  _Static_assert((e % SECTOR_SIZE_BYTES) == 0, "Filesystem end region not sector aligned"); \

#define FILE_SYSTEM_FS_REGION_ENTRY_CONSTRUCTOR(s, e) { .start = s, .end = e },

// Make sure all the filesystem regions are flash sector aligned
FILE_SYSTEM_REGIONS(FILE_SYSTEM_LAYOUT_CHECK)

// Build the flash region list
static const FSRegion s_region_list[] = {
  FILE_SYSTEM_REGIONS(FILE_SYSTEM_FS_REGION_ENTRY_CONSTRUCTOR)
};

//! Erase all the regions that belong to our filesystem. Note that this is just a flash erase,
//! if you want to leave behind a fully erased and initialized filesystem you should be using
//! pfs_format instead.
void filesystem_regions_erase_all(void);
