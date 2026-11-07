/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#include <pbl/kernel/mpu.h>

#define STATIC_SLOTS (CONFIG_KERNEL_MPU_REGIONS - ARCH_THREAD_MPU_REGIONS)

// SF32LB52: SystemInit() programs slots 0..4.
#ifdef CONFIG_SOC_SF32LB52
#define FIRST_SLOT 5U
#else
#define FIRST_SLOT 0U
#endif

static const struct pbl_mpu_region *s_static_regions[STATIC_SLOTS];
static unsigned int s_num_static_regions = FIRST_SLOT;

static bool prv_overlaps(uintptr_t base, size_t size, uintptr_t other_base, size_t other_size) {
  return base < other_base + other_size && other_base < base + size;
}

int pbl_mpu_region_add(const struct pbl_mpu_region *region) {
  for (unsigned int i = FIRST_SLOT; i < s_num_static_regions; i++) {
    const struct pbl_mpu_region *other = s_static_regions[i];
    if (prv_overlaps(region->base, region->size, other->base, other->size)) {
      return -EINVAL;
    }
  }
  if (s_num_static_regions == STATIC_SLOTS) {
    return -ENOSPC;
  }

  arch_mpu_region_set(s_num_static_regions, region);
  s_static_regions[s_num_static_regions++] = region;
  return 0;
}

void pbl_mpu_regions_enable(void) {
  arch_mpu_enable();
}

bool pbl_mpu_region_is_stack_guard(uintptr_t start, uintptr_t end) {
  for (unsigned int i = FIRST_SLOT; i < s_num_static_regions; i++) {
    const struct pbl_mpu_region *region = s_static_regions[i];
    if ((region->attr & PBL_MPU_ACCESS_MSK) == PBL_MPU_NO_ACCESS &&
        prv_overlaps(start, end - start, region->base, region->size)) {
      return true;
    }
  }

  for (unsigned int i = PBL_THREAD_REGION_STACK_GUARD; i < ARCH_THREAD_MPU_REGIONS; i++) {
    struct pbl_mpu_region region;
    if (arch_mpu_region_get(STATIC_SLOTS + i, &region) &&
        prv_overlaps(start, end - start, region.base, region.size)) {
      return true;
    }
  }
  return false;
}

static const char *prv_access_str(uint32_t attr) {
  switch (attr & PBL_MPU_ACCESS_MSK) {
    case PBL_MPU_NO_ACCESS:
      return "none";
    case PBL_MPU_P_RW:
      return "P:RW";
    case PBL_MPU_P_RW_U_RO:
      return "P:RW U:RO";
    case PBL_MPU_P_RW_U_RW:
      return "P:RW U:RW";
    case PBL_MPU_P_RO:
      return "P:RO";
    case PBL_MPU_P_RO_U_RO:
      return "P:RO U:RO";
    default:
      return "?";
  }
}

void pbl_mpu_regions_print(void (*print)(void *ctx, const char *line), void *ctx) {
  static const char *const s_thread_names[ARCH_THREAD_MPU_REGIONS] = {
    [0] = "thread 0",
    [1] = "thread 1",
    [PBL_THREAD_REGION_STACK_GUARD] = "stack guard",
    [PBL_THREAD_REGION_STACK_GUARD + 1] = "syscall guard",
  };
  char line[80];

  for (unsigned int i = 0; i < CONFIG_KERNEL_MPU_REGIONS; i++) {
    struct pbl_mpu_region region;
    if (!arch_mpu_region_get(i, &region)) {
      snprintf(line, sizeof(line), "%2u -", i);
    } else {
      const char *name = "?";
      if (i >= STATIC_SLOTS) {
        name = s_thread_names[i - STATIC_SLOTS];
      } else if (s_static_regions[i] != nullptr) {
        name = s_static_regions[i]->name;
        region.attr = s_static_regions[i]->attr;
      }
      snprintf(line, sizeof(line), "%2u %-14s 0x%08" PRIxPTR " 0x%08zx %s %s", i, name, region.base,
               region.size, (region.attr & PBL_MPU_EXEC) ? "X" : "-", prv_access_str(region.attr));
    }
    print(ctx, line);
  }
}
