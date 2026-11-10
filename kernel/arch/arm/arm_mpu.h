/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mpu.h>

#include <cmsis_core.h>

//! Encodes a region for a slot, into what the RBAR and RASR (ARMv7-M) or RLAR
//! (ARMv8-M) registers take.
void arm_mpu_region_encode(unsigned int slot, const struct pbl_mpu_region *region, uint32_t *rbar,
                           uint32_t *attr);

//! Writes a word of the user read-only kernel data in the MPU slot given.
static PBL_ALWAYS_INLINE void arm_mpu_user_ro_write32(int slot, uint32_t *dst, uint32_t val) {
#ifdef CONFIG_ARMV8_M_MAINLINE
  if (slot >= 0) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t rnr = MPU->RNR;
    MPU->RNR = (uint32_t)slot;
    const uint32_t rlar = MPU->RLAR;
    MPU->RLAR = rlar & ~MPU_RLAR_EN_Msk;
    __DSB();
    __ISB();
    *dst = val;
    __DSB();
    MPU->RLAR = rlar;
    MPU->RNR = rnr;
    __DSB();
    __ISB();
    __set_PRIMASK(primask);
    return;
  }
#else
  (void)slot;
#endif
  *dst = val;
}
