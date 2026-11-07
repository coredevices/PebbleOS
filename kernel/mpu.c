/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#include <pbl/kernel/mpu.h>
#include <pbl/util/size.h>

#define STATIC_SLOTS (CONFIG_KERNEL_MPU_REGIONS - ARCH_THREAD_MPU_REGIONS)

static const struct pbl_mpu_region *s_static_regions[STATIC_SLOTS];
static unsigned int s_num_static_regions;
static int s_user_ro_slot = -1;

static bool prv_overlaps(uintptr_t base, size_t size, uintptr_t other_base, size_t other_size) {
  return base < other_base + other_size && other_base < base + size;
}

int pbl_mpu_region_add(const struct pbl_mpu_region *region) {
  for (unsigned int i = 0; i < s_num_static_regions; i++) {
    const struct pbl_mpu_region *other = s_static_regions[i];
    if (prv_overlaps(region->base, region->size, other->base, other->size)) {
      return -EINVAL;
    }
  }
  if (s_num_static_regions == STATIC_SLOTS) {
    return -ENOSPC;
  }

  if ((region->attr & PBL_MPU_ACCESS_MSK) == PBL_MPU_P_RW_U_RO) {
    PBL_ASSERTN(s_user_ro_slot < 0);
    s_user_ro_slot = s_num_static_regions;
  }

  arch_mpu_region_set(s_num_static_regions, region);
  s_static_regions[s_num_static_regions++] = region;
  return 0;
}

#ifndef CONFIG_ARCH_POSIX
extern const uint8_t __unpriv_ro_bss_start__[];
extern const uint8_t __unpriv_ro_bss_size__[];
extern const uint8_t __isr_stack_start__[];
extern const uint8_t __FLASH_start__[];
extern const uint8_t __FLASH_size__[];

static const struct pbl_mpu_region s_kernel_regions[] = {
  {
    .name = "firmware",
#ifdef CONFIG_ARMV8_M_MAINLINE
    .base = CONFIG_FLASH_BASE + CONFIG_FLASH_OFFSET,
    .size = CONFIG_FW_FLASH_SIZE,
#else
    // ARMv7-M cannot line a region up with the image.
    .base = (uintptr_t)__FLASH_start__,
    .size = (size_t)__FLASH_size__,
#endif
    .attr = PBL_MPU_P_RO_U_RO | PBL_MPU_NORMAL_WT | PBL_MPU_EXEC,
  },
  {
    // Written through the default memory map, which it has to stay coherent with.
    .name = "ro bss",
    .base = (uintptr_t)__unpriv_ro_bss_start__,
    .size = (size_t)__unpriv_ro_bss_size__,
    .attr = PBL_MPU_P_RW_U_RO | PBL_MPU_NORMAL_WB,
  },
#if CONFIG_KERNEL_STACK_GUARD_SIZE > 0
  {
    .name = "isr guard",
    .base = (uintptr_t)__isr_stack_start__,
    .size = CONFIG_KERNEL_STACK_GUARD_SIZE,
    .attr = PBL_MPU_NO_ACCESS | PBL_MPU_NORMAL_NC,
  },
#endif
};
#endif

[[maybe_unused]] static void prv_regions_add(const struct pbl_mpu_region *regions, size_t num) {
  for (size_t i = 0; i < num; i++) {
    PBL_ASSERTN(pbl_mpu_region_add(&regions[i]) == 0);
  }
}

void mpu_init(void) {
  arch_mpu_init();
#ifndef CONFIG_ARCH_POSIX
  prv_regions_add(s_kernel_regions, ARRAY_LENGTH(s_kernel_regions));
#endif
#ifdef CONFIG_SOC_HAS_MPU_REGIONS
  prv_regions_add(pbl_soc_mpu_regions, pbl_soc_num_mpu_regions);
#endif
  arch_mpu_enable();
}

void pbl_mpu_user_ro_write(void *dst, const void *src, size_t n) {
  arch_mpu_user_ro_write(s_user_ro_slot, dst, src, n);
}

const struct pbl_mpu_region *pbl_mpu_region_firmware(void) {
  static const struct pbl_mpu_region s_firmware = {
    .name = "firmware",
    .base = CONFIG_FLASH_BASE + CONFIG_FLASH_OFFSET,
    .size = CONFIG_FW_FLASH_SIZE,
  };
  return &s_firmware;
}

bool pbl_mpu_region_is_stack_guard(uintptr_t start, uintptr_t end) {
  for (unsigned int i = 0; i < s_num_static_regions; i++) {
    const struct pbl_mpu_region *region = s_static_regions[i];
    if ((region->attr & PBL_MPU_ACCESS_MSK) == PBL_MPU_NO_ACCESS &&
        prv_overlaps(start, end - start, region->base, region->size)) {
      return true;
    }
  }

  for (unsigned int i = THREAD_REGION_STACK_GUARD; i < ARCH_THREAD_MPU_REGIONS; i++) {
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
    [THREAD_REGION_STACK_GUARD] = "stack guard",
    [THREAD_REGION_SYSCALL_GUARD] = "syscall guard",
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
      } else if (i < s_num_static_regions) {
        name = s_static_regions[i]->name != nullptr ? s_static_regions[i]->name : "?";
        region.attr = s_static_regions[i]->attr;
      }
      snprintf(line, sizeof(line), "%2u %-14s 0x%08" PRIxPTR " 0x%08zx %s %s", i, name, region.base,
               region.size, (region.attr & PBL_MPU_EXEC) ? "X" : "-", prv_access_str(region.attr));
    }
    print(ctx, line);
  }
}
