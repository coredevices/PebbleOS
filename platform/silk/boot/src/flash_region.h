/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Scratch space for firmware images (normal and recovery).
// We assume this is 64k aligned...
#define FLASH_REGION_FIRMWARE_SCRATCH_BEGIN 0x000000
#define FLASH_REGION_FIRMWARE_SCRATCH_END 0x100000 // 1024k

#define FLASH_REGION_SAFE_FIRMWARE_BEGIN 0x200000
#define FLASH_REGION_SAFE_FIRMWARE_END 0x280000 // 512k
