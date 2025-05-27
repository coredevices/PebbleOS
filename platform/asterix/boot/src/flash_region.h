#pragma once

// Scratch space for firmware images (normal and recovery).
// We assume this is 64k aligned...
#define FLASH_REGION_FIRMWARE_SCRATCH_BEGIN 0x0100000
#define FLASH_REGION_FIRMWARE_SCRATCH_END   0x01FFFFF // 1024k

#define FLASH_REGION_SAFE_FIRMWARE_BEGIN 0x0000000
#define FLASH_REGION_SAFE_FIRMWARE_END   0x007FFFF // 512k
