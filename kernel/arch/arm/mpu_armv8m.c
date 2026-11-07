/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "arm_mpu.h"

#include <string.h>

#include <pbl/util/size.h>

#include <cmsis_core.h>
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
      ARM_MPU_ATTR(ARM_MPU_ATTR_NON_CACHEABLE, ARM_MPU_ATTR_NON_CACHEABLE),
  [TYPE_IDX(PBL_MPU_NORMAL_WT)] =
      ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(1, 0, 1, 0), ARM_MPU_ATTR_MEMORY_(1, 0, 1, 0)),
  [TYPE_IDX(PBL_MPU_NORMAL_WB)] =
      ARM_MPU_ATTR(ARM_MPU_ATTR_MEMORY_(1, 1, 1, 1), ARM_MPU_ATTR_MEMORY_(1, 1, 1, 1)),
  [TYPE_IDX(PBL_MPU_DEVICE)] = ARM_MPU_ATTR(ARM_MPU_ATTR_DEVICE, ARM_MPU_ATTR_DEVICE_nGnRnE),
};

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
  PBL_ASSERTN(((MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos) >=
              CONFIG_KERNEL_MPU_REGIONS);

  ARM_MPU_Disable();
  for (uint32_t i = 0U; i < CONFIG_KERNEL_MPU_REGIONS; i++) {
    ARM_MPU_ClrRegion(i);
  }
  for (uint8_t i = 0U; i < ARRAY_LENGTH(s_type_to_mair); i++) {
    ARM_MPU_SetMemAttr(i, s_type_to_mair[i]);
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
  (void)slot;
  PBL_ASSERTN((region->base & 0x1f) == 0 && (region->size & 0x1f) == 0);
  PBL_ASSERTN((region->attr & PBL_MPU_ACCESS_MSK) < ARRAY_LENGTH(s_access_to_ap));

  const uint32_t sh = (region->attr & PBL_MPU_SHAREABLE) ? ARM_MPU_SH_INNER : ARM_MPU_SH_NON;
  const uint32_t ap = s_access_to_ap[region->attr & PBL_MPU_ACCESS_MSK];
  const uint32_t xn = (region->attr & PBL_MPU_EXEC) ? 0U : 1U;

  *rbar = (region->base & MPU_RBAR_BASE_Msk) | (sh << MPU_RBAR_SH_Pos) | (ap << MPU_RBAR_AP_Pos) |
          (xn << MPU_RBAR_XN_Pos);
  *attr = ((region->base + region->size - 1U) & MPU_RLAR_LIMIT_Msk) |
          ((TYPE_IDX(region->attr)) << MPU_RLAR_AttrIndx_Pos) | MPU_RLAR_EN_Msk;
}

void arch_mpu_region_set(unsigned int slot, const struct pbl_mpu_region *region) {
  uint32_t rbar, rlar;
  arm_mpu_region_encode(slot, region, &rbar, &rlar);

  // A context switch also goes through RNR.
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  ARM_MPU_SetRegion(slot, rbar, rlar);
  __DSB();
  __ISB();
  __set_PRIMASK(primask);
}

void arch_mpu_user_ro_write(int slot, void *dst, const void *src, size_t n) {
  if (slot < 0) {
    memcpy(dst, src, n);
    return;
  }

  const uint32_t primask = __get_PRIMASK();
  __disable_irq();

  const uint32_t rnr = MPU->RNR;
  MPU->RNR = (uint32_t)slot;
  const uint32_t rlar = MPU->RLAR;
  MPU->RLAR = rlar & ~MPU_RLAR_EN_Msk;
  __DSB();
  __ISB();

  memcpy(dst, src, n);

  __DSB();
  MPU->RLAR = rlar;
  MPU->RNR = rnr;
  __DSB();
  __ISB();

  __set_PRIMASK(primask);
}

bool arch_mpu_region_get(unsigned int slot, struct pbl_mpu_region *region) {
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  MPU->RNR = slot;
  const uint32_t rbar = MPU->RBAR;
  const uint32_t rlar = MPU->RLAR;
  __set_PRIMASK(primask);

  if ((rlar & MPU_RLAR_EN_Msk) == 0U) {
    return false;
  }

  *region = (struct pbl_mpu_region){
    .base = rbar & MPU_RBAR_BASE_Msk,
    .size = (rlar & MPU_RLAR_LIMIT_Msk) - (rbar & MPU_RBAR_BASE_Msk) + 0x20U,
    .attr = prv_ap_to_access((rbar & MPU_RBAR_AP_Msk) >> MPU_RBAR_AP_Pos) |
            ((rbar & MPU_RBAR_XN_Msk) ? 0U : PBL_MPU_EXEC),
  };
  return true;
}
