/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/mpu.h"

#include "system/passert.h"
#include "util/size.h"

#include <inttypes.h>

#include <cmsis_core.h>

// ARMv8-M has a 2-bit AP field, which cannot express two of the
// MpuPermissions values precisely:
//   - NoAccess: there is no "deny everything" encoding; we degrade to
//     AP=0b10 (RO priv only), the same encoding used for PrivRO. Don't
//     rely on this for thread-stack-overflow detection -- use PSPLIM.
//   - PrivRW_UserRO: the AP field cannot split write access by privilege.
//     We pick AP=0b01 (R/W any privilege level), which gives the user
//     write access too. See the MpuPermissions doc in drivers/mpu.h.
static const uint8_t s_permission_to_ap[MpuPermissionsCount] = {
  [MpuPermissions_NoAccess]      = 0x2,
  [MpuPermissions_PrivRW]        = 0x0,
  [MpuPermissions_PrivRW_UserRO] = 0x1,
  [MpuPermissions_PrivRW_UserRW] = 0x1,
  [MpuPermissions_PrivRO]        = 0x2,
  [MpuPermissions_PrivRO_UserRO] = 0x3,
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
  PBL_ASSERTN(region->permissions < MpuPermissionsCount);
  return s_permission_to_ap[region->permissions];
}

static MpuPermissions decode_permission_value(uint8_t ap) {
  // ARMv8-M's two-bit AP loses information: AP=0b10 could have been set
  // for either NoAccess or PrivRO. Decode to PrivRO canonically.
  switch (ap & 0x3) {
    case 0x0: return MpuPermissions_PrivRW;
    case 0x1: return MpuPermissions_PrivRW_UserRW;
    case 0x2: return MpuPermissions_PrivRO;
    case 0x3: return MpuPermissions_PrivRO_UserRO;
    default:  return MpuPermissions_NoAccess;
  }
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
  region.permissions = decode_permission_value(access_permissions);

  region.size = (rlar & MPU_RLAR_LIMIT_Msk) - region.base_address + 0x20;
  region.enabled = (rlar & MPU_RLAR_EN_Msk) != 0;
  region.cache_policy = (rlar & MPU_RLAR_AttrIndx_Msk) >> MPU_RLAR_AttrIndx_Pos;

  return region;
}
