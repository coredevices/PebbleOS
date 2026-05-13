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
  uint8_t value:2;
} PermissionMapping;

static const PermissionMapping s_permission_mappings[] = {
  // NOTE(1): we cannot have all accesses disabled, keep RO by privileged code only.
  // NOTE(2): we cannot have different write access for priv/unpriv, allow R/W to any level
  { false, false, false, false, 0x2 }, // AP=0b10: RO by privileged code only (1)
  { true,  true,  false, false, 0x0 }, // AP=0b00: R/W by privileged code only
  { true,  true,  true,  false, 0x1 }, // AP=0b01: R/W by any privilege level (2)
  { true,  true,  true,  true,  0x1 }, // AP=0b01: R/W by any privilege level
  { true,  false, false, false, 0x2 }, // AP=0b10: RO by privileged code only
  { true,  false, true,  false, 0x3 }, // AP=0b11: RO by by any privilege level
};

static const uint32_t s_cache_settings[] = {
  [MpuCachePolicy_NotCacheable] = ARM_MPU_ATTR(ARM_MPU_ATTR_NON_CACHEABLE,
                                               ARM_MPU_ATTR_NON_CACHEABLE),
  [MpuCachePolicy_WriteThrough] = ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(1, 0, 1, 0),
                                               ARM_MPU_ATTR_MEMORY_(1, 0, 1, 0)),
  [MpuCachePolicy_WriteBackWriteAllocate] = ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(1, 1, 1, 1),
                                                         ARM_MPU_ATTR_MEMORY_(1, 1, 1, 1)),
  [MpuCachePolicy_WriteBackNoWriteAllocate] = ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(1, 1, 0, 1),
                                                           ARM_MPU_ATTR_MEMORY_(1, 1, 0, 1)),
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

void mpu_enable(void) {
  ARM_MPU_SetMemAttr(MpuCachePolicy_NotCacheable,
                     s_cache_settings[MpuCachePolicy_NotCacheable]);
  ARM_MPU_SetMemAttr(MpuCachePolicy_WriteThrough,
                     s_cache_settings[MpuCachePolicy_WriteThrough]);
  ARM_MPU_SetMemAttr(MpuCachePolicy_WriteBackWriteAllocate,
                     s_cache_settings[MpuCachePolicy_WriteBackWriteAllocate]);
  ARM_MPU_SetMemAttr(MpuCachePolicy_WriteBackNoWriteAllocate,
                     s_cache_settings[MpuCachePolicy_WriteBackNoWriteAllocate]);

  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
}

void mpu_get_register_settings(const MpuRegion* region, uint32_t *base_address_reg,
                               uint32_t *attributes_reg) {
  PBL_ASSERTN(region);
  PBL_ASSERTN((region->base_address & 0x1f) == 0);
  PBL_ASSERTN((region->region_num & ~0xf) == 0);
  PBL_ASSERTN((region->cache_policy < ARRAY_LENGTH(s_cache_settings)));
  PBL_ASSERTN((region->size & 0x1f) == 0);

  *base_address_reg = ((region->base_address & MPU_RBAR_BASE_Msk) |
                       ((ARM_MPU_SH_INNER << MPU_RBAR_SH_Pos) & MPU_RBAR_SH_Msk) |
                       ((get_permission_value(region) << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk));
  *attributes_reg = (((region->base_address + region->size - 1U) & MPU_RLAR_LIMIT_Msk) |
                     ((region->cache_policy << MPU_RLAR_AttrIndx_Pos) & MPU_RLAR_AttrIndx_Msk) |
                     ((region->enabled << MPU_RLAR_EN_Pos) & MPU_RLAR_EN_Msk));
}

void mpu_set_region(const MpuRegion* region) {
  uint32_t base_reg, attr_reg;

  mpu_get_register_settings(region, &base_reg, &attr_reg);

  ARM_MPU_SetRegion(region->region_num, base_reg, attr_reg);
}

MpuRegion mpu_get_region(int region_num) {
  MpuRegion region;
  uint32_t rbar, rlar;
  uint8_t access_permissions;

  region.region_num = region_num;

  MPU->RNR = region_num;
  rbar = MPU->RBAR;
  rlar = MPU->RLAR;

  region.base_address = rbar & MPU_RBAR_BASE_Msk;

  access_permissions = (rbar & MPU_RBAR_AP_Msk) >> MPU_RBAR_AP_Pos;
  for (size_t i = 0; i < ARRAY_LENGTH(s_permission_mappings); ++i) {
    if (s_permission_mappings[i].value == access_permissions) {
      region.priv_read = s_permission_mappings[i].priv_read;
      region.priv_write = s_permission_mappings[i].priv_write;
      region.user_read = s_permission_mappings[i].user_read;
      region.user_write = s_permission_mappings[i].user_write;
      break;
    }
  }

  region.size = (rlar & MPU_RLAR_LIMIT_Msk) - region.base_address + 0x20;
  region.enabled = (rlar & MPU_RLAR_EN_Msk) != 0;
  region.cache_policy = (rlar & MPU_RLAR_AttrIndx_Msk) >> MPU_RLAR_AttrIndx_Pos;

  return region;
}
