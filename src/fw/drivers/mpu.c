/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mpu.h"

#include "mcu/cache.h"
#include "system/passert.h"

#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

#include <cmsis_core.h>

extern const uint32_t __SRAM_size__[];
#if !defined(SRAM_BASE)
#if defined(MICRO_FAMILY_NRF52)
#include <drivers/nrfx_common.h>
#define SRAM_BASE (0x20000000UL)
#elif defined(MICRO_FAMILY_SF32LB52)
#define SRAM_BASE (0x20000000UL)
#endif
#endif
#define SRAM_END (SRAM_BASE + (uint32_t)__SRAM_size__)

void mpu_disable(void) {
  ARM_MPU_Disable();
}

// Fill in the task parameters for a new task with the configurable memory regions we want.
void mpu_set_task_configurable_regions(MemoryRegion_t *memory_regions,
                                       const MpuRegion **region_ptrs) {
  unsigned int region_num, region_idx;
  uint32_t base_reg, attr_reg;

  // Setup the configurable MPU regions
  for (region_num=portFIRST_CONFIGURABLE_REGION, region_idx=0; region_num <= portLAST_CONFIGURABLE_REGION;
            region_num++, region_idx++) {
    const MpuRegion *mpu_region = region_ptrs[region_idx];
    MpuRegion unused_region = {};

    // If not region defined, use unused
    if (mpu_region == NULL) {
      mpu_region = &unused_region;
      attr_reg = 0; // Has a 0 in the enable bit, so this region won't be enabled.
    } else {
      // Make sure that the region numbers passed in jive with the configurable region numbers.
      PBL_ASSERTN(mpu_region->region_num == region_num);
      // Our FreeRTOS port makes the assumption that the ulParameters field contains exactly what
      // should be placed into the MPU_RASR register. It will figure out the MPU_RBAR from the
      // pvBaseAddress field.
      mpu_get_register_settings(mpu_region, &base_reg, &attr_reg);
    }

    memory_regions[region_idx] = (MemoryRegion_t) {
      .pvBaseAddress = (void *)mpu_region->base_address,
      .ulLengthInBytes = mpu_region->size,
      .ulParameters = attr_reg,
    };
  }

}

bool mpu_memory_is_cachable(const void *addr) {
  if (!dcache_is_enabled()) {
    return false;
  }
  // TODO PBL-37601: We're assuming only SRAM is cachable for now for simplicity sake. We should
  // account for MPU configuration and also the fact that memory-mapped QSPI access goes through the
  // cache.
  return ((uint32_t)addr >= SRAM_BASE) && ((uint32_t)addr < SRAM_END);
}

void mpu_init_region_from_region(MpuRegion *copy, const MpuRegion *from, bool allow_user_access) {
  *copy = *from;
  copy->user_read = allow_user_access;
  copy->user_write = allow_user_access;
}
