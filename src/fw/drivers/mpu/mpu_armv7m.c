/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/mpu.h"

#include "system/passert.h"
#include "util/size.h"

#include <inttypes.h>

#include <cmsis_core.h>

typedef struct PermissionMapping {
  bool priv_read:1;
  bool priv_write:1;
  bool user_read:1;
  bool user_write:1;
  uint8_t value:3;
} PermissionMapping;

static const PermissionMapping s_permission_mappings[] = {
  { false, false, false, false, 0x0 },
  { true,  true,  false, false, 0x1 },
  { true,  true,  true,  false, 0x2 },
  { true,  true,  true,  true,  0x3 },
  { true,  false, false, false, 0x5 },
  { true,  false, true,  false, 0x6 },
  { true,  false, true,  false, 0x7 } // Both 0x6 and 0x7 map to the same permissions.
};

static const uint32_t s_cache_settings[] = {
  [MpuCachePolicy_NotCacheable] = (0x1 << MPU_RASR_TEX_Pos) | (MPU_RASR_S_Msk),
  [MpuCachePolicy_WriteThrough] = (MPU_RASR_S_Msk | MPU_RASR_C_Msk),
  [MpuCachePolicy_WriteBackWriteAllocate] =
      (0x1 << MPU_RASR_TEX_Pos) | (MPU_RASR_S_Msk | MPU_RASR_C_Msk | MPU_RASR_B_Msk),
  [MpuCachePolicy_WriteBackNoWriteAllocate] =
      (MPU_RASR_S_Msk | MPU_RASR_C_Msk | MPU_RASR_B_Msk),
};

static uint8_t get_permission_value(const MpuRegion* region) {
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_permission_mappings); ++i) {
    if (s_permission_mappings[i].priv_read == region->priv_read &&
        s_permission_mappings[i].priv_write == region->priv_write &&
        s_permission_mappings[i].user_read == region->user_read &&
        s_permission_mappings[i].user_write == region->user_write) {
      return s_permission_mappings[i].value;
    }
  }
  WTF;
  return 0;
}

static uint32_t get_size_field(const MpuRegion* region) {
  unsigned int size = 32;
  int result = 4;
  while (size != region->size) {
    PBL_ASSERT(size < region->size || size == 0x400000, "Invalid region size: %"PRIu32,
               region->size);

    size *= 2;
    ++result;
  }

  return result;
}

void mpu_enable(void) {
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
}

// Get the required region base address and region attribute register settings for the given region.
// These are the values which should written to the RBAR and RASR registers to configure that
// region.
void mpu_get_register_settings(const MpuRegion* region, uint32_t *base_address_reg,
                               uint32_t *attributes_reg) {
  PBL_ASSERTN(region);
  PBL_ASSERTN((region->base_address & 0x1f) == 0);
  PBL_ASSERTN((region->region_num & ~0xf) == 0);
  PBL_ASSERTN((region->cache_policy < ARRAY_LENGTH(s_cache_settings)));

  // MPU Region Base Address Register
  // | Addr (27 bits) | Region Valid Bit | Region Num (4 bits) |
  // The address is unshifted, we take the top bits of the address and assume everything below
  // is zero, since the address must be power of 2 size aligned.
  *base_address_reg = region->base_address |
              0x1 << 4 |
              region->region_num;

  // MPU Region Attribute and Size Register
  // A lot of stuff here! Split into bytes...
  // | Reserved (3 bits) | XN Bit | Reserved Bit | Permission Field (3 bits) |
  // | Reserved (2 bits) | TEX (3 bits) | S | C | B |
  // | Subregion Disable Byte |
  // | Reserved (2 bits) | Size Field (5 bits) | Enable Bit |
  *attributes_reg = (get_permission_value(region) << 24) |
              s_cache_settings[region->cache_policy] |
              region->disabled_subregions << 8 |      // Disabled subregions
              (get_size_field(region) << 1) |
              region->enabled; // Enabled
}

void mpu_set_region(const MpuRegion* region) {
  uint32_t base_reg, attr_reg;

  mpu_get_register_settings(region, &base_reg, &attr_reg);

  MPU->RBAR = base_reg;
  MPU->RASR = attr_reg;
}

MpuRegion mpu_get_region(int region_num) {
  MpuRegion region = { .region_num = region_num };

  MPU->RNR = region_num;

  const uint32_t attributes = MPU->RASR;

  region.enabled = attributes & 0x1;

  if (region.enabled) {
    const uint8_t size_field = (attributes >> 1) & 0x1f;
    region.size = 32 << (size_field - 4);

    region.disabled_subregions = (attributes & 0x0000ff00) >> 8;

    const uint32_t raw_base_address = MPU->RBAR;
    region.base_address = raw_base_address & ~(region.size - 1);

    const uint8_t access_permissions = (attributes >> 24) & 0x7;

    for (unsigned int i = 0; i < ARRAY_LENGTH(s_permission_mappings); ++i) {
      if (s_permission_mappings[i].value == access_permissions) {
        region.priv_read = s_permission_mappings[i].priv_read;
        region.priv_write = s_permission_mappings[i].priv_write;
        region.user_read = s_permission_mappings[i].user_read;
        region.user_write = s_permission_mappings[i].user_write;
        break;
      }
    }
  }

  return region;
}
