/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define PAGE_SIZE_BYTES                             (0x100)

#define SECTOR_SIZE_BYTES                           (0x10000)
#define SECTOR_ADDR_MASK                            (~(SECTOR_SIZE_BYTES - 1))

#define SUBSECTOR_SIZE_BYTES                        (0x1000)
#define SUBSECTOR_ADDR_MASK                         (~(SUBSECTOR_SIZE_BYTES - 1))


// A bit of preprocessor magic to help with automatically calculating flash region addresses
//////////////////////////////////////////////////////////////////////////////

#define FLASH_REGION_DEF(MACRO, arg) \
  MACRO(FIRMWARE_SCRATCH,         0x200000 /* 2048k */, arg) \
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

#define FLASH_REGION_FIRMWARE_SCRATCH_BEGIN FLASH_REGION_START_ADDR(FIRMWARE_SCRATCH)
#define FLASH_REGION_FIRMWARE_SCRATCH_END FLASH_REGION_END_ADDR(FIRMWARE_SCRATCH)

#define FLASH_REGION_SAFE_FIRMWARE_BEGIN FLASH_REGION_START_ADDR(SAFE_FIRMWARE)
#define FLASH_REGION_SAFE_FIRMWARE_END FLASH_REGION_END_ADDR(SAFE_FIRMWARE)

#define FLASH_REGION_MFG_INFO_BEGIN FLASH_REGION_START_ADDR(MFG_INFO)
#define FLASH_REGION_MFG_INFO_END FLASH_REGION_END_ADDR(MFG_INFO)

#define BOARD_NOR_FLASH_SIZE FLASH_REGION_START_ADDR(_COUNT)


// Static asserts to make sure everything worked out
//////////////////////////////////////////////////////////////////////////////

// make sure all the sizes are multiples of the subsector size (4k)
FLASH_REGION_SIZE_CHECK(SUBSECTOR_SIZE_BYTES)

// make sure the total size is what we expect (16MB for robert)
_Static_assert(BOARD_NOR_FLASH_SIZE == 0x1000000, "Flash size should be 16MB");
