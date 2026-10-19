/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/mcu/cache.h>
#include <pbl/mcu/mpu.h>

#include <cmsis_core.h>
#include <kernel.h>

extern const uint32_t __SRAM_size__[];
#if !defined(SRAM_BASE)
#if defined(CONFIG_SOC_NRF52)
#include <drivers/nrfx_common.h>
#define SRAM_BASE (0x20000000UL)
#elifdef CONFIG_SOC_SF32LB52
#define SRAM_BASE (0x20000000UL)
#endif
#endif
#define SRAM_END (SRAM_BASE + (uint32_t)__SRAM_size__)

void mpu_disable(void) {
  ARM_MPU_Disable();
}

bool mpu_memory_is_cachable(const void *addr) {
  if (!dcache_is_enabled()) {
    return false;
  }
  // TODO: only SRAM is assumed to be cachable. Account for the MPU configuration and for
  // memory-mapped QSPI access going through the cache.
  return ((uint32_t)addr >= SRAM_BASE) && ((uint32_t)addr < SRAM_END);
}

void mpu_init_region_from_region(MpuRegion *copy, const MpuRegion *from, bool allow_user_access) {
  // Caller-side invariant: `from` is a PrivRW region (App/Worker RAM).
  // Toggle user RW based on which task is about to run.
  PBL_ASSERTN(from->permissions == MpuPermissions_PrivRW);
  *copy = *from;
  copy->permissions = allow_user_access ? MpuPermissions_PrivRW_UserRW : MpuPermissions_PrivRW;
}
