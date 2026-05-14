/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/mpu.h"

#include "system/passert.h"
#include "util/size.h"

#include <inttypes.h>

#include <cmsis_core.h>

// ARMv7-M has a 3-bit AP field, so every MpuPermissions value maps to a
// unique encoding (0x4 is reserved; 0x6/0x7 both decode to "RO any priv",
// the table picks 0x6 canonically and mpu_get_region() accepts either).
static const uint8_t s_permission_to_ap[MpuPermissionsCount] = {
  [MpuPermissions_NoAccess]      = 0x0,
  [MpuPermissions_PrivRW]        = 0x1,
  [MpuPermissions_PrivRW_UserRO] = 0x2,
  [MpuPermissions_PrivRW_UserRW] = 0x3,
  [MpuPermissions_PrivRO]        = 0x5,
  [MpuPermissions_PrivRO_UserRO] = 0x6,
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
  PBL_ASSERTN(region->permissions < MpuPermissionsCount);
  return s_permission_to_ap[region->permissions];
}

static MpuPermissions decode_permission_value(uint8_t ap) {
  // 0x4 is reserved; 0x7 aliases 0x6 (PrivRO_UserRO).
  switch (ap & 0x7) {
    case 0x0: return MpuPermissions_NoAccess;
    case 0x1: return MpuPermissions_PrivRW;
    case 0x2: return MpuPermissions_PrivRW_UserRO;
    case 0x3: return MpuPermissions_PrivRW_UserRW;
    case 0x5: return MpuPermissions_PrivRO;
    case 0x6:
    case 0x7: return MpuPermissions_PrivRO_UserRO;
    default:  return MpuPermissions_NoAccess;
  }
}

// ARMv7-M MPU regions must be power-of-two sized and naturally aligned. To
// cover an arbitrary (base, size) range we pick the smallest power-of-two
// block (32B..4MB) that:
//   - covers [base, base+size),
//   - is naturally aligned to its own size,
//   - has [base, base+size) starting and ending on subregion boundaries
//     (subregion = block/8).
// The 8-bit subregion-disable mask then masks off the parts of the block
// that fall outside [base, base+size).
#define MPU_ARMV7M_MIN_REGION_SIZE  32u
#define MPU_ARMV7M_MAX_REGION_SIZE  (4u * 1024u * 1024u)
#define MPU_ARMV7M_NUM_SUBREGIONS   8u

typedef struct {
  uintptr_t block_base;
  uint32_t block_size;
  uint8_t disabled_subregions;
} BlockLayout;

static BlockLayout compute_block_layout(uintptr_t base, uint32_t size) {
  PBL_ASSERT(size >= MPU_ARMV7M_MIN_REGION_SIZE, "MPU region too small: %"PRIu32, size);

  uint32_t block = MPU_ARMV7M_MIN_REGION_SIZE;
  while (block < size) {
    block <<= 1;
  }
  while (block <= MPU_ARMV7M_MAX_REGION_SIZE) {
    const uint32_t subregion = block / MPU_ARMV7M_NUM_SUBREGIONS;
    const uintptr_t start_in_block = base & (block - 1);
    const uintptr_t end_in_block = start_in_block + size;
    if ((start_in_block & (subregion - 1)) == 0 &&
        (end_in_block & (subregion - 1)) == 0 &&
        end_in_block <= block) {
      const unsigned start_sub = start_in_block / subregion;
      const unsigned end_sub = end_in_block / subregion;
      uint8_t mask = 0xFF;
      for (unsigned i = start_sub; i < end_sub; i++) {
        mask &= (uint8_t)~(1u << i);
      }
      return (BlockLayout) {
        .block_base = base - start_in_block,
        .block_size = block,
        .disabled_subregions = mask,
      };
    }
    block <<= 1;
  }
  PBL_CROAK("MPU region cannot fit subregion alignment: base=0x%08"PRIxPTR" size=0x%"PRIx32,
            base, size);
}

static uint32_t get_size_field(uint32_t block_size) {
  unsigned int size = MPU_ARMV7M_MIN_REGION_SIZE;
  int result = 4;
  while (size != block_size) {
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

  const BlockLayout layout = compute_block_layout(region->base_address, region->size);

  // MPU Region Base Address Register
  // | Addr (27 bits) | Region Valid Bit | Region Num (4 bits) |
  // The address is unshifted, we take the top bits of the address and assume everything below
  // is zero, since the address must be power of 2 size aligned.
  *base_address_reg = layout.block_base |
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
              layout.disabled_subregions << 8 |
              (get_size_field(layout.block_size) << 1) |
              region->enabled;
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
    const uint32_t block_size = 32u << (size_field - 4);
    const uint8_t disabled_subregions = (attributes & 0x0000ff00) >> 8;
    const uint32_t raw_base_address = MPU->RBAR;
    const uintptr_t block_base = raw_base_address & ~(block_size - 1);

    // Decode the (contiguous) enabled-subregion range back into a real
    // (base, size). Matches the layout produced by compute_block_layout().
    const uint32_t subregion = block_size / MPU_ARMV7M_NUM_SUBREGIONS;
    unsigned start_sub = 0;
    while (start_sub < MPU_ARMV7M_NUM_SUBREGIONS && (disabled_subregions & (1u << start_sub))) {
      start_sub++;
    }
    unsigned end_sub = start_sub;
    while (end_sub < MPU_ARMV7M_NUM_SUBREGIONS && !(disabled_subregions & (1u << end_sub))) {
      end_sub++;
    }
    region.base_address = block_base + (uintptr_t)start_sub * subregion;
    region.size = (end_sub - start_sub) * subregion;

    const uint8_t access_permissions = (attributes >> 24) & 0x7;
    region.permissions = decode_permission_value(access_permissions);
  }

  return region;
}
