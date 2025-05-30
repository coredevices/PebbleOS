/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
