/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "freertos_types.h"

typedef enum MpuCachePolicy {
  // FIXME(SF32LB52): system_bf0_ap.c uses now up to 4 attributes as MPU is not fully implemented.
#ifdef MICRO_FAMILY_SF32LB52
  MpuCachePolicy_Reserved0,
  MpuCachePolicy_Reserved1,
  MpuCachePolicy_Reserved2,
  MpuCachePolicy_Reserved3,
#endif
  MpuCachePolicy_NotCacheable,
  MpuCachePolicy_WriteThrough,
  MpuCachePolicy_WriteBackWriteAllocate,
  MpuCachePolicy_WriteBackNoWriteAllocate,
} MpuCachePolicy;

// Describes a memory region by its real (base, size). The ARMv7-M backend
// internally rounds up to a power-of-two block and computes the subregion
// mask required by the hardware; the ARMv8-M backend programs the limit
// register directly. Callers never see these implementation details.
typedef struct MpuRegion {
  uint8_t region_num:4;
  bool enabled:1;
  uintptr_t base_address;
  uint32_t size;
  MpuCachePolicy cache_policy;
  bool priv_read:1;
  bool priv_write:1;
  bool user_read:1;
  bool user_write:1;
} MpuRegion;

void mpu_enable(void);

void mpu_disable(void);

void mpu_set_region(const MpuRegion* region);

MpuRegion mpu_get_region(int region_num);

void mpu_get_register_settings(const MpuRegion* region, uint32_t *base_address_reg,
                               uint32_t *attributes_reg);

void mpu_set_task_configurable_regions(MemoryRegion_t *memory_regions,
                                       const MpuRegion **region_ptrs);

bool mpu_memory_is_cachable(const void *addr);

void mpu_init_region_from_region(MpuRegion *copy, const MpuRegion *from, bool allow_user_access);
