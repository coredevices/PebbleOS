/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_mpu.h"

#include <string.h>

#include <pbl/util/size.h>

#include <kernel.h>

#define TYPE_IDX(attr) (((attr) & PBL_MPU_TYPE_MSK) >> 3)

// The 2-bit AP field cannot express two of the access attributes:
//   - NO_ACCESS: AP=0b10 (RO priv only). Unprivileged code and privileged
//     writes still fault, which is what a stack guard needs.
//   - P_RW_U_RO: the AP field cannot split write access by privilege.
//     We pick AP=0b11 (RO any privilege level) and privileged writers go
//     through arch_mpu_user_ro_write().
static const uint8_t s_access_to_ap[] = {
  [PBL_MPU_NO_ACCESS] = 0x2, [PBL_MPU_P_RW] = 0x0, [PBL_MPU_P_RW_U_RO] = 0x3,
  [PBL_MPU_P_RW_U_RW] = 0x1, [PBL_MPU_P_RO] = 0x2, [PBL_MPU_P_RO_U_RO] = 0x3,
};

static const uint8_t s_type_to_mair[] = {
  [TYPE_IDX(PBL_MPU_NORMAL_NC)] =
      ARM_MPU_MAIR_ATTR(ARM_MPU_MAIR_NON_CACHEABLE, ARM_MPU_MAIR_NON_CACHEABLE),
  [TYPE_IDX(PBL_MPU_NORMAL_WT)] = ARM_MPU_MAIR_ATTR(ARM_MPU_MAIR_WT_RA, ARM_MPU_MAIR_WT_RA),
  [TYPE_IDX(PBL_MPU_NORMAL_WB)] = ARM_MPU_MAIR_ATTR(ARM_MPU_MAIR_WB_RA_WA, ARM_MPU_MAIR_WB_RA_WA),
  [TYPE_IDX(PBL_MPU_DEVICE)] = ARM_MPU_MAIR_ATTR(0U, ARM_MPU_MAIR_DEVICE_NGNRNE),
};

static_assert(ARRAY_LENGTH(s_type_to_mair) <= 4, "memory types must fit in MAIR0");

static uint32_t prv_ap_to_access(uint32_t ap) {
  // AP=0b10 could have been set for NO_ACCESS or P_RO, and AP=0b11 for
  // P_RW_U_RO or P_RO_U_RO. Static regions report what they were added with.
  switch (ap) {
    case 0x0:
      return PBL_MPU_P_RW;
    case 0x1:
      return PBL_MPU_P_RW_U_RW;
    case 0x2:
      return PBL_MPU_P_RO;
    default:
      return PBL_MPU_P_RO_U_RO;
  }
}

void arch_mpu_init(void) {
  PBL_ASSERTN(ARM_MPU_TYPE_DREGION(arch_readl(ARM_MPU_TYPE)) >= CONFIG_KERNEL_MPU_REGIONS);

  arm_mpu_disable();
  for (uint32_t i = 0U; i < CONFIG_KERNEL_MPU_REGIONS; i++) {
    arch_writel(i, ARM_MPU_RNR);
    arch_writel(0U, ARM_MPU_RLAR);
  }
  uint32_t mair0 = 0U;
  for (unsigned int i = 0U; i < ARRAY_LENGTH(s_type_to_mair); i++) {
    mair0 |= (uint32_t)s_type_to_mair[i] << (i * 8U);
  }
  arch_writel(mair0, ARM_MPU_MAIR(0));
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
  (void)slot;
  PBL_ASSERTN((region->base & 0x1f) == 0 && (region->size & 0x1f) == 0);
  PBL_ASSERTN((region->attr & PBL_MPU_ACCESS_MSK) < ARRAY_LENGTH(s_access_to_ap));

  const uint32_t sh =
      (region->attr & PBL_MPU_SHAREABLE) ? ARM_MPU_RBAR_SH_INNER : ARM_MPU_RBAR_SH_NON;
  const uint32_t ap = s_access_to_ap[region->attr & PBL_MPU_ACCESS_MSK];
  const uint32_t xn = (region->attr & PBL_MPU_EXEC) ? 0U : 1U;

  *rbar = (region->base & ARM_MPU_RBAR_BASE_MSK) | (sh << ARM_MPU_RBAR_SH_POS) |
          (ap << ARM_MPU_RBAR_AP_POS) | (xn ? ARM_MPU_RBAR_XN : 0U);
  *attr = ((region->base + region->size - 1U) & ARM_MPU_RLAR_LIMIT_MSK) |
          ((TYPE_IDX(region->attr)) << ARM_MPU_RLAR_ATTRINDX_POS) | ARM_MPU_RLAR_EN;
}

void arch_mpu_region_set(unsigned int slot, const struct pbl_mpu_region *region) {
  uint32_t rbar, rlar;
  arm_mpu_region_encode(slot, region, &rbar, &rlar);

  // A context switch also goes through RNR.
  const uint32_t primask = arm_primask_get();
  arm_irq_disable();
  arch_writel(slot, ARM_MPU_RNR);
  arch_writel(rbar, ARM_MPU_RBAR);
  arch_writel(rlar, ARM_MPU_RLAR);
  arm_dsb();
  arm_isb();
  arm_primask_set(primask);
}

void arch_mpu_user_ro_write(int slot, void *dst, const void *src, size_t n) {
  if (slot < 0) {
    memcpy(dst, src, n);
    return;
  }

  const uint32_t primask = arm_primask_get();
  arm_irq_disable();

  const uint32_t rnr = arch_readl(ARM_MPU_RNR);
  arch_writel((uint32_t)slot, ARM_MPU_RNR);
  const uint32_t rlar = arch_readl(ARM_MPU_RLAR);
  arch_writel(rlar & ~ARM_MPU_RLAR_EN, ARM_MPU_RLAR);
  arm_dsb();
  arm_isb();

  memcpy(dst, src, n);

  arm_dsb();
  arch_writel(rlar, ARM_MPU_RLAR);
  arch_writel(rnr, ARM_MPU_RNR);
  arm_dsb();
  arm_isb();

  arm_primask_set(primask);
}

bool arch_mpu_region_get(unsigned int slot, struct pbl_mpu_region *region) {
  const uint32_t primask = arm_primask_get();
  arm_irq_disable();
  arch_writel(slot, ARM_MPU_RNR);
  const uint32_t rbar = arch_readl(ARM_MPU_RBAR);
  const uint32_t rlar = arch_readl(ARM_MPU_RLAR);
  arm_primask_set(primask);

  if ((rlar & ARM_MPU_RLAR_EN) == 0U) {
    return false;
  }

  *region = (struct pbl_mpu_region){
    .base = rbar & ARM_MPU_RBAR_BASE_MSK,
    .size = (rlar & ARM_MPU_RLAR_LIMIT_MSK) - (rbar & ARM_MPU_RBAR_BASE_MSK) + 0x20U,
    .attr = prv_ap_to_access((rbar & ARM_MPU_RBAR_AP_MSK) >> ARM_MPU_RBAR_AP_POS) |
            ((rbar & ARM_MPU_RBAR_XN) ? 0U : PBL_MPU_EXEC),
  };
  return true;
}
