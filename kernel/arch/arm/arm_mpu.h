/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "arm_cpu.h"

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mpu.h>

//! Encodes a region for a slot, into what the RBAR and RASR (ARMv7-M) or RLAR
//! (ARMv8-M) registers take.
void arm_mpu_region_encode(unsigned int slot, const struct pbl_mpu_region *region, uint32_t *rbar,
                           uint32_t *attr);

static inline void arm_mpu_disable(void) {
  arm_dmb();
  arch_writel(arch_readl(ARM_SCB_SHCSR) & ~ARM_SCB_SHCSR_MEMFAULTENA, ARM_SCB_SHCSR);
  arch_writel(arch_readl(ARM_MPU_CTRL) & ~ARM_MPU_CTRL_ENABLE, ARM_MPU_CTRL);
  arm_dsb();
  arm_isb();
}

static inline void arm_mpu_enable(uint32_t ctrl) {
  arm_dmb();
  arch_writel(ctrl | ARM_MPU_CTRL_ENABLE, ARM_MPU_CTRL);
  arch_writel(arch_readl(ARM_SCB_SHCSR) | ARM_SCB_SHCSR_MEMFAULTENA, ARM_SCB_SHCSR);
  arm_dsb();
  arm_isb();
}

//! Writes a word of the user read-only kernel data in the MPU slot given.
static PBL_ALWAYS_INLINE void arm_mpu_user_ro_write32(int slot, uint32_t *dst, uint32_t val) {
#ifdef CONFIG_ARMV8_M_MAINLINE
  if (slot >= 0) {
    const uint32_t primask = arm_primask_get();
    arm_irq_disable();
    const uint32_t rnr = arch_readl(ARM_MPU_RNR);
    arch_writel((uint32_t)slot, ARM_MPU_RNR);
    const uint32_t rlar = arch_readl(ARM_MPU_RLAR);
    arch_writel(rlar & ~ARM_MPU_RLAR_EN, ARM_MPU_RLAR);
    arm_dsb();
    arm_isb();
    *dst = val;
    arm_dsb();
    arch_writel(rlar, ARM_MPU_RLAR);
    arch_writel(rnr, ARM_MPU_RNR);
    arm_dsb();
    arm_isb();
    arm_primask_set(primask);
    return;
  }
#else
  (void)slot;
#endif
  *dst = val;
}
