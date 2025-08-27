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

#define SECTOR_SIZE_BYTES 0x10000
#define SECTOR_ADDR_MASK (~(SECTOR_SIZE_BYTES - 1))

#define SUBSECTOR_SIZE_BYTES 0x1000
#define SUBSECTOR_ADDR_MASK (~(SUBSECTOR_SIZE_BYTES - 1))
#define FLASH_FILESYSTEM_BLOCK_SIZE SUBSECTOR_SIZE_BYTES

// Filesystem layout
///////////////////////////////////////

// Scratch space for firmware images (normal and recovery).
// We assume this is 64k aligned...
#define FLASH_REGION_FIRMWARE_SLOT_1_BEGIN 0x0
#define FLASH_REGION_FIRMWARE_SLOT_1_END 0x80000 // 512k

// Formerly FLASH_REGION_APP_BEGIN
#define FLASH_REGION_FILESYSTEM_3_BEGIN 0x80000
#define FLASH_REGION_FILESYSTEM_3_END 0x100000 // 512k

// Formerly REGISTRY_FLASH
// Use one sector for the shared prf storage
#define FLASH_REGION_SHARED_PRF_STORAGE_BEGIN 0x100000
#define FLASH_REGION_SHARED_PRF_STORAGE_END   0x110000 // 64k

// Use one sector to store the factory settings registry
#define FACTORY_REGISTRY_FLASH_BEGIN 0x110000 //
#define FACTORY_REGISTRY_FLASH_END 0x120000 // 64k

#define FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN 0x120000
#define FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END   0x160000 // 256k

// Formerly part of the 640k of data-logging, which was added to the filesystem as FILESYSTEM_5
// Reserved a couple sectors because it's always nice to have a couple free.
#define FLASH_REGION_UNUSED0_BEGIN 0x160000
#define FLASH_REGION_UNUSED0_END 0x180000 // 128k

#define FLASH_REGION_FILESYSTEM_5_BEGIN 0x180000
#define FLASH_REGION_FILESYSTEM_5_END 0x200000 // 512k

#define FLASH_REGION_SAFE_FIRMWARE_BEGIN 0x200000
#define FLASH_REGION_SAFE_FIRMWARE_END 0x280000 // 512k

#define FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN 0x280000
#define FLASH_REGION_SYSTEM_RESOURCES_BANK_1_END 0x2c0000 // 256k

// Formerly FLASH_REGION_RESERVED_BEGIN
#define FLASH_REGION_FILESYSTEM_2_BEGIN 0x2c0000  //64k
#define FLASH_REGION_FILESYSTEM_2_END 0x2d0000

#define FLASH_REGION_FILESYSTEM_BEGIN 0x2d0000
#define FLASH_REGION_FILESYSTEM_END 0x320000 // 320k

// Formerly FLASH_REGION_APP_RESOURCES_BEGIN
#define FLASH_REGION_FILESYSTEM_4_BEGIN 0x320000
#define FLASH_REGION_FILESYSTEM_4_END 0x3e0000 // 768k

#define FLASH_REGION_DEBUG_DB_BEGIN 0x3e0000
#define FLASH_REGION_DEBUG_DB_END 0x400000 // 128k
#define FLASH_DEBUG_DB_BLOCK_SIZE SECTOR_SIZE_BYTES

#define FLASH_REGION_EXTRA_FILESYSTEM_BEGIN 0x400000
#define FLASH_REGION_EXTRA_FILESYSTEM_END BOARD_NOR_FLASH_SIZE

// 0x400000 is the end of the SPI flash address space.

