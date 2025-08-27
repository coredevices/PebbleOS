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

#pragma once

#define PAGE_SIZE_BYTES                             (0x100)

#define SECTOR_SIZE_BYTES                           (0x10000)
#define SECTOR_ADDR_MASK                            (~(SECTOR_SIZE_BYTES - 1))

#define SUBSECTOR_SIZE_BYTES                        (0x1000)
#define SUBSECTOR_ADDR_MASK                         (~(SUBSECTOR_SIZE_BYTES - 1))


// A bit of preprocessor magic to help with automatically calculating flash region addresses
//////////////////////////////////////////////////////////////////////////////

#define FLASH_REGION_DEF(MACRO, arg) \
  MACRO(FIRMWARE_SLOT_1,          0x200000 /* 2048k */, arg) \
  MACRO(SYSTEM_RESOURCES_BANK_0,  0x100000 /* 1024k */, arg) \
  MACRO(SYSTEM_RESOURCES_BANK_1,  0x100000 /* 1024k */, arg) \
  MACRO(SAFE_FIRMWARE,            0x080000 /* 512k */, arg) \
  MACRO(DEBUG_DB,                 0x020000 /* 128k */, arg) \
  MACRO(MFG_INFO,                 0x020000 /* 128k */, arg) \
  MACRO(FILESYSTEM,               0xB30000 /* 11456k */, arg) \
  MACRO(RSVD,                     0x00F000 /* 60k */, arg) \
  MACRO(SHARED_PRF_STORAGE,       0x001000 /* 4k */, arg)

#include "flash_region_def_helper.h"


// Flash region _BEGIN and _END addresses
//////////////////////////////////////////////////////////////////////////////

#define FLASH_REGION_FIRMWARE_SLOT_1_BEGIN FLASH_REGION_START_ADDR(FIRMWARE_SLOT_1)
#define FLASH_REGION_FIRMWARE_SLOT_1_END FLASH_REGION_END_ADDR(FIRMWARE_SLOT_1)

#define FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN FLASH_REGION_START_ADDR(SYSTEM_RESOURCES_BANK_0)
#define FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END FLASH_REGION_END_ADDR(SYSTEM_RESOURCES_BANK_0)

#define FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN FLASH_REGION_START_ADDR(SYSTEM_RESOURCES_BANK_1)
#define FLASH_REGION_SYSTEM_RESOURCES_BANK_1_END FLASH_REGION_END_ADDR(SYSTEM_RESOURCES_BANK_1)

#define FLASH_REGION_SAFE_FIRMWARE_BEGIN FLASH_REGION_START_ADDR(SAFE_FIRMWARE)
#define FLASH_REGION_SAFE_FIRMWARE_END FLASH_REGION_END_ADDR(SAFE_FIRMWARE)

#define FLASH_REGION_DEBUG_DB_BEGIN FLASH_REGION_START_ADDR(DEBUG_DB)
#define FLASH_REGION_DEBUG_DB_END FLASH_REGION_END_ADDR(DEBUG_DB)
#define FLASH_DEBUG_DB_BLOCK_SIZE SUBSECTOR_SIZE_BYTES

#define FLASH_REGION_FILESYSTEM_BEGIN FLASH_REGION_START_ADDR(FILESYSTEM)
#define FLASH_REGION_FILESYSTEM_END FLASH_REGION_END_ADDR(FILESYSTEM)
#define FLASH_FILESYSTEM_BLOCK_SIZE 0x2000  // 8k

#define FLASH_REGION_SHARED_PRF_STORAGE_BEGIN FLASH_REGION_START_ADDR(SHARED_PRF_STORAGE)
#define FLASH_REGION_SHARED_PRF_STORAGE_END FLASH_REGION_END_ADDR(SHARED_PRF_STORAGE)

#define FLASH_REGION_MFG_INFO_BEGIN FLASH_REGION_START_ADDR(MFG_INFO)
#define FLASH_REGION_MFG_INFO_END FLASH_REGION_END_ADDR(MFG_INFO)

#define BOARD_NOR_FLASH_SIZE FLASH_REGION_START_ADDR(_COUNT)


// Static asserts to make sure everything worked out
//////////////////////////////////////////////////////////////////////////////

// make sure all the sizes are multiples of the subsector size (4k)
FLASH_REGION_SIZE_CHECK(SUBSECTOR_SIZE_BYTES)

// make sure the shared PRF storage is within the last 64k sector so we can protect them.
_Static_assert(FLASH_REGION_SHARED_PRF_STORAGE_BEGIN >= BOARD_NOR_FLASH_SIZE - SECTOR_SIZE_BYTES,
              "Shared PRF storage should be within the last 64k of flash");

// make sure the total size is what we expect (16MB for robert)
_Static_assert(BOARD_NOR_FLASH_SIZE == 0x1000000, "Flash size should be 16MB");
