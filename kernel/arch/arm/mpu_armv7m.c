/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_mpu.h"

#include <string.h>

#include <pbl/util/size.h>

#include <cmsis_core.h>
#include <kernel.h>

#define TYPE_IDX(attr) (((attr) & PBL_MPU_TYPE_MSK) >> 3)

// ARMv7-M has a 3-bit AP field, so every access attribute maps to a unique
// encoding (0x4 is reserved; 0x6/0x7 both decode to "RO any priv").
static const uint8_t s_access_to_ap[] = {
  [PBL_MPU_NO_ACCESS] = 0x0, [PBL_MPU_P_RW] = 0x1, [PBL_MPU_P_RW_U_RO] = 0x2,
  [PBL_MPU_P_RW_U_RW] = 0x3, [PBL_MPU_P_RO] = 0x5, [PBL_MPU_P_RO_U_RO] = 0x6,
};

static const uint32_t s_type_to_tex_c_b[] = {
  [TYPE_IDX(PBL_MPU_NORMAL_NC)] = (0x1 << MPU_RASR_TEX_Pos),
  [TYPE_IDX(PBL_MPU_NORMAL_WT)] = MPU_RASR_C_Msk,
  [TYPE_IDX(PBL_MPU_NORMAL_WB)] = (0x1 << MPU_RASR_TEX_Pos) | MPU_RASR_C_Msk | MPU_RASR_B_Msk,
  [TYPE_IDX(PBL_MPU_DEVICE)] = MPU_RASR_B_Msk,
};

static uint32_t prv_ap_to_access(uint32_t ap) {
  switch (ap) {
    case 0x1:
      return PBL_MPU_P_RW;
    case 0x2:
      return PBL_MPU_P_RW_U_RO;
    case 0x3:
      return PBL_MPU_P_RW_U_RW;
    case 0x5:
      return PBL_MPU_P_RO;
    case 0x6:
    case 0x7:
      return PBL_MPU_P_RO_U_RO;
    default:
      return PBL_MPU_NO_ACCESS;
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
#define MPU_ARMV7M_MIN_REGION_SIZE 32u
#define MPU_ARMV7M_MAX_REGION_SIZE (4u * 1024u * 1024u)
#define MPU_ARMV7M_NUM_SUBREGIONS  8u

typedef struct {
  uintptr_t block_base;
  uint32_t block_size;
  uint8_t disabled_subregions;
} BlockLayout;

static BlockLayout compute_block_layout(uintptr_t base, uint32_t size) {
  PBL_ASSERTN(size >= MPU_ARMV7M_MIN_REGION_SIZE);

  uint32_t block = MPU_ARMV7M_MIN_REGION_SIZE;
  while (block < size) {
    block <<= 1;
  }
  while (block <= MPU_ARMV7M_MAX_REGION_SIZE) {
    const uint32_t subregion = block / MPU_ARMV7M_NUM_SUBREGIONS;
    const uintptr_t start_in_block = base & (block - 1);
    const uintptr_t end_in_block = start_in_block + size;
    if ((start_in_block & (subregion - 1)) == 0 && (end_in_block & (subregion - 1)) == 0 &&
        end_in_block <= block) {
      const unsigned start_sub = start_in_block / subregion;
      const unsigned end_sub = end_in_block / subregion;
      uint8_t mask = 0xFF;
      for (unsigned i = start_sub; i < end_sub; i++) {
        mask &= (uint8_t)~(1u << i);
      }
      return (BlockLayout){
        .block_base = base - start_in_block,
        .block_size = block,
        .disabled_subregions = mask,
      };
    }
    block <<= 1;
  }
  PBL_ASSERTN(false);
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

void arch_mpu_init(void) {
  PBL_ASSERTN(((MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos) >=
              CONFIG_KERNEL_MPU_REGIONS);

  ARM_MPU_Disable();
  for (uint32_t i = 0U; i < CONFIG_KERNEL_MPU_REGIONS; i++) {
    ARM_MPU_ClrRegion(i);
  }
}

void arch_mpu_enable(void) {
#ifdef CONFIG_KERNEL_MPU_IN_FAULTS
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_HFNMIENA_Msk);
#else
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
#endif
}

void arm_mpu_region_encode(unsigned int slot, const struct pbl_mpu_region *region, uint32_t *rbar,
                           uint32_t *attr) {
  PBL_ASSERTN((region->base & 0x1f) == 0);
  PBL_ASSERTN((region->attr & PBL_MPU_ACCESS_MSK) < ARRAY_LENGTH(s_access_to_ap));

  const BlockLayout layout = compute_block_layout(region->base, region->size);

  *rbar = layout.block_base | MPU_RBAR_VALID_Msk | slot;
  *attr = (((region->attr & PBL_MPU_EXEC) ? 0U : 1U) << MPU_RASR_XN_Pos) |
          ((uint32_t)s_access_to_ap[region->attr & PBL_MPU_ACCESS_MSK] << MPU_RASR_AP_Pos) |
          s_type_to_tex_c_b[TYPE_IDX(region->attr)] |
          ((region->attr & PBL_MPU_SHAREABLE) ? MPU_RASR_S_Msk : 0U) |
          ((uint32_t)layout.disabled_subregions << MPU_RASR_SRD_Pos) |
          (get_size_field(layout.block_size) << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;
}

void arch_mpu_region_set(unsigned int slot, const struct pbl_mpu_region *region) {
  uint32_t rbar, rasr;
  arm_mpu_region_encode(slot, region, &rbar, &rasr);

  // A context switch also goes through RNR, which RBAR's VALID bit updates.
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  MPU->RBAR = rbar;
  MPU->RASR = rasr;
  __DSB();
  __ISB();
  __set_PRIMASK(primask);
}

void arch_mpu_user_ro_write(int slot, void *dst, const void *src, size_t n) {
  (void)slot;
  memcpy(dst, src, n);
}

bool arch_mpu_region_get(unsigned int slot, struct pbl_mpu_region *region) {
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  MPU->RNR = slot;
  const uint32_t rbar = MPU->RBAR;
  const uint32_t rasr = MPU->RASR;
  __set_PRIMASK(primask);

  if ((rasr & MPU_RASR_ENABLE_Msk) == 0U) {
    return false;
  }

  const uint32_t size_field = (rasr & MPU_RASR_SIZE_Msk) >> MPU_RASR_SIZE_Pos;
  const uint32_t block_size = 32U << (size_field - 4U);
  const uint32_t disabled_subregions = (rasr & MPU_RASR_SRD_Msk) >> MPU_RASR_SRD_Pos;

  // Decode the contiguous enabled subregions, as compute_block_layout() lays them out.
  const uint32_t subregion = block_size / MPU_ARMV7M_NUM_SUBREGIONS;
  unsigned int start_sub = 0U;
  while (start_sub < MPU_ARMV7M_NUM_SUBREGIONS && (disabled_subregions & (1U << start_sub))) {
    start_sub++;
  }
  unsigned int end_sub = start_sub;
  while (end_sub < MPU_ARMV7M_NUM_SUBREGIONS && !(disabled_subregions & (1U << end_sub))) {
    end_sub++;
  }

  *region = (struct pbl_mpu_region){
    .base = (rbar & ~(block_size - 1U)) + start_sub * subregion,
    .size = (end_sub - start_sub) * subregion,
    .attr = prv_ap_to_access((rasr & MPU_RASR_AP_Msk) >> MPU_RASR_AP_Pos) |
            ((rasr & MPU_RASR_XN_Msk) ? 0U : PBL_MPU_EXEC),
  };
  return true;
}
