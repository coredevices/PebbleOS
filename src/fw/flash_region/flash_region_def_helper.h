/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

enum {
#define FLASH_REGION_LIST(name, size, arg) FlashRegion_##name,
  FLASH_REGION_DEF(FLASH_REGION_LIST, NULL)
  FlashRegion__COUNT
};

#define FLASH_REGION_ADDR_HELPER(name, size, tgt) \
    + (FlashRegion_##name < (tgt) ? (size) : 0)

#ifndef FLASH_REGION_BASE_ADDRESS
#define FLASH_REGION_BASE_ADDRESS 0
#endif

// These macros add up all the sizes of the flash regions that come before (and including in the
// case of the _END_ADDR macro) the specified one to determine the proper flash address value.
#define FLASH_REGION_START_ADDR(region) \
  (((0) FLASH_REGION_DEF(FLASH_REGION_ADDR_HELPER, FlashRegion_##region)) + FLASH_REGION_BASE_ADDRESS)
#define FLASH_REGION_END_ADDR(region) \
  (((0) FLASH_REGION_DEF(FLASH_REGION_ADDR_HELPER, FlashRegion_##region + 1)) + FLASH_REGION_BASE_ADDRESS)

// Checks that all regions are a multiple of the specified size (usually sector or subsector size)
#define FLASH_REGION_SIZE_CHECK_HELPER(name, size, arg) \
  && ((size) % (arg) == 0)
#define FLASH_REGION_SIZE_CHECK(size) \
  _Static_assert((1) FLASH_REGION_DEF(FLASH_REGION_SIZE_CHECK_HELPER, size), "Invalid region size");
