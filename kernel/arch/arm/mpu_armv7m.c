/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_mpu.h"

#include <string.h>

#include <pbl/util/size.h>

#include <kernel.h>

#define TYPE_IDX(attr) (((attr) & PBL_MPU_TYPE_MSK) >> 3)

// ARMv7-M has a 3-bit AP field, so every access attribute maps to a unique
// encoding (0x4 is reserved; 0x6/0x7 both decode to "RO any priv").
static const uint8_t s_access_to_ap[] = {
  [PBL_MPU_NO_ACCESS] = 0x0, [PBL_MPU_P_RW] = 0x1, [PBL_MPU_P_RW_U_RO] = 0x2,
  [PBL_MPU_P_RW_U_RW] = 0x3, [PBL_MPU_P_RO] = 0x5, [PBL_MPU_P_RO_U_RO] = 0x6,
};

static const uint32_t s_type_to_tex_c_b[] = {
  [TYPE_IDX(PBL_MPU_NORMAL_NC)] = (0x1 << ARM_MPU_RASR_TEX_POS),
  [TYPE_IDX(PBL_MPU_NORMAL_WT)] = ARM_MPU_RASR_C,
  [TYPE_IDX(PBL_MPU_NORMAL_WB)] = (0x1 << ARM_MPU_RASR_TEX_POS) | ARM_MPU_RASR_C | ARM_MPU_RASR_B,
  [TYPE_IDX(PBL_MPU_DEVICE)] = ARM_MPU_RASR_B,
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
  PBL_ASSERTN(ARM_MPU_TYPE_DREGION(arch_readl(ARM_MPU_TYPE)) >= CONFIG_KERNEL_MPU_REGIONS);

  arm_mpu_disable();
  for (uint32_t i = 0U; i < CONFIG_KERNEL_MPU_REGIONS; i++) {
    arch_writel(i, ARM_MPU_RNR);
    arch_writel(0U, ARM_MPU_RASR);
  }
}

void arch_mpu_enable(void) {
#ifdef CONFIG_KERNEL_MPU_IN_FAULTS
  arm_mpu_enable(ARM_MPU_CTRL_PRIVDEFENA | ARM_MPU_CTRL_HFNMIENA);
#else
  arm_mpu_enable(ARM_MPU_CTRL_PRIVDEFENA);
#endif
}

void arm_mpu_region_encode(unsigned int slot, const struct pbl_mpu_region *region, uint32_t *rbar,
                           uint32_t *attr) {
  PBL_ASSERTN((region->base & 0x1f) == 0);
  PBL_ASSERTN((region->attr & PBL_MPU_ACCESS_MSK) < ARRAY_LENGTH(s_access_to_ap));

  const BlockLayout layout = compute_block_layout(region->base, region->size);

  *rbar = layout.block_base | ARM_MPU_RBAR_VALID | slot;
  *attr = ((region->attr & PBL_MPU_EXEC) ? 0U : ARM_MPU_RASR_XN) |
          ((uint32_t)s_access_to_ap[region->attr & PBL_MPU_ACCESS_MSK] << ARM_MPU_RASR_AP_POS) |
          s_type_to_tex_c_b[TYPE_IDX(region->attr)] |
          ((region->attr & PBL_MPU_SHAREABLE) ? ARM_MPU_RASR_S : 0U) |
          ((uint32_t)layout.disabled_subregions << ARM_MPU_RASR_SRD_POS) |
          (get_size_field(layout.block_size) << ARM_MPU_RASR_SIZE_POS) | ARM_MPU_RASR_ENABLE;
}

void arch_mpu_region_set(unsigned int slot, const struct pbl_mpu_region *region) {
  uint32_t rbar, rasr;
  arm_mpu_region_encode(slot, region, &rbar, &rasr);

  // A context switch also goes through RNR, which RBAR's VALID bit updates.
  const uint32_t primask = arm_primask_get();
  arm_irq_disable();
  arch_writel(rbar, ARM_MPU_RBAR);
  arch_writel(rasr, ARM_MPU_RASR);
  arm_dsb();
  arm_isb();
  arm_primask_set(primask);
}

void arch_mpu_user_ro_write(int slot, void *dst, const void *src, size_t n) {
  (void)slot;
  memcpy(dst, src, n);
}

bool arch_mpu_region_get(unsigned int slot, struct pbl_mpu_region *region) {
  const uint32_t primask = arm_primask_get();
  arm_irq_disable();
  arch_writel(slot, ARM_MPU_RNR);
  const uint32_t rbar = arch_readl(ARM_MPU_RBAR);
  const uint32_t rasr = arch_readl(ARM_MPU_RASR);
  arm_primask_set(primask);

  if ((rasr & ARM_MPU_RASR_ENABLE) == 0U) {
    return false;
  }

  const uint32_t size_field = (rasr & ARM_MPU_RASR_SIZE_MSK) >> ARM_MPU_RASR_SIZE_POS;
  const uint32_t block_size = 32U << (size_field - 4U);
  const uint32_t disabled_subregions = (rasr & ARM_MPU_RASR_SRD_MSK) >> ARM_MPU_RASR_SRD_POS;

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
    .attr = prv_ap_to_access((rasr & ARM_MPU_RASR_AP_MSK) >> ARM_MPU_RASR_AP_POS) |
            ((rasr & ARM_MPU_RASR_XN) ? 0U : PBL_MPU_EXEC),
  };
  return true;
}
