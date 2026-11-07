/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "memory_layout.h"

#include <pbl/kernel/assert.h>
#include <pbl/kernel/thread.h>

#include <flash_region/flash_region.h>
#include <logging/logging_private.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

static void prv_log_line(void *ctx, const char *line) {
  char buffer[90];
  PBL_LOG_FROM_FAULT_HANDLER_FMT(buffer, sizeof(buffer), "%s", line);
}

void memory_layout_dump_mpu_regions(void) {
  pbl_mpu_regions_print(prv_log_line, nullptr);
}

#if defined(UNITTEST) || defined(CONFIG_ARCH_POSIX)
static const uint32_t __unpriv_ro_bss_start__ = 0;
static const uint32_t __unpriv_ro_bss_size__ = 0;

static const uint32_t __APP_RAM__ = 0;
static const uint32_t __APP_RAM_size__ = 0;
static const uint32_t __WORKER_RAM__ = 0;
static const uint32_t __WORKER_RAM_size__ = 0;

static const uint32_t __FLASH_start__ = 0;
static const uint32_t __FLASH_size__ = 0;
#else
extern const uint32_t __unpriv_ro_bss_start__[];
extern const uint32_t __unpriv_ro_bss_size__[];
extern const uint32_t __isr_stack_start__[];

extern const uint32_t __APP_RAM__[];
extern const uint32_t __APP_RAM_size__[];
extern const uint32_t __WORKER_RAM__[];
extern const uint32_t __WORKER_RAM_size__[];

extern const uint32_t __FLASH_start__[];
extern const uint32_t __FLASH_size__[];
#endif

// Kernel read only RAM. Parts of RAM that it's kosher for unprivileged apps to read
static const struct pbl_mpu_region s_readonly_bss_region = {
  .name = "ro bss",
  .base = (uintptr_t)__unpriv_ro_bss_start__,
  .size = (size_t)__unpriv_ro_bss_size__,
  .attr = PBL_MPU_P_RW_U_RO | PBL_MPU_NORMAL_WB | PBL_MPU_SHAREABLE,
};

#if CONFIG_KERNEL_STACK_GUARD_SIZE > 0
static const struct pbl_mpu_region s_isr_stack_guard_region = {
  .name = "isr guard",
  .base = (uintptr_t)__isr_stack_start__,
  .size = CONFIG_KERNEL_STACK_GUARD_SIZE,
  .attr = PBL_MPU_NO_ACCESS | PBL_MPU_NORMAL_NC,
};
#endif

// App and worker .text is relocated into their RAM.
static const struct pbl_mpu_region s_app_region = {
  .name = "app",
  .base = (uintptr_t)__APP_RAM__,
  .size = (size_t)__APP_RAM_size__,
  .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_WB | PBL_MPU_EXEC | PBL_MPU_SHAREABLE,
};

static const struct pbl_mpu_region s_worker_region = {
  .name = "worker",
  .base = (uintptr_t)__WORKER_RAM__,
  .size = (size_t)__WORKER_RAM_size__,
  .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_WB | PBL_MPU_EXEC | PBL_MPU_SHAREABLE,
};

static const struct pbl_mpu_region s_microflash_region = {
  .name = "flash",
  .base = (uintptr_t)__FLASH_start__,
  .size = (size_t)__FLASH_size__,
  .attr = PBL_MPU_P_RO_U_RO | PBL_MPU_NORMAL_WT | PBL_MPU_EXEC | PBL_MPU_SHAREABLE,
};

#if defined(CONFIG_SOC_SF32LB52) && defined(CONFIG_MMAP_RESOURCES)
static_assert(FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END ==
                  FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN,
              "resource banks must be contiguous");

// System resources are served as pointers into the memory mapped banks.
static const struct pbl_mpu_region s_resource_banks_region = {
  .name = "resource banks",
  .base = FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN,
  .size = FLASH_REGION_SYSTEM_RESOURCES_BANK_1_END - FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN,
  .attr = PBL_MPU_P_RO_U_RO | PBL_MPU_NORMAL_WT | PBL_MPU_SHAREABLE,
};
#endif

void memory_layout_setup_mpu(void) {
  // On SF32LB52 SystemInit() already programs a user-RO region for the firmware image.
#ifndef CONFIG_SOC_SF32LB52
  PBL_ASSERTN(pbl_mpu_region_add(&s_microflash_region) == 0);
#endif
#if defined(CONFIG_SOC_SF32LB52) && defined(CONFIG_MMAP_RESOURCES)
  PBL_ASSERTN(pbl_mpu_region_add(&s_resource_banks_region) == 0);
#endif
  PBL_ASSERTN(pbl_mpu_region_add(&s_readonly_bss_region) == 0);
#if CONFIG_KERNEL_STACK_GUARD_SIZE > 0
  PBL_ASSERTN(pbl_mpu_region_add(&s_isr_stack_guard_region) == 0);
#endif

  pbl_mpu_regions_enable();
}

const struct pbl_mpu_region *memory_layout_get_app_region(void) {
  return &s_app_region;
}

const struct pbl_mpu_region *memory_layout_get_readonly_bss_region(void) {
  return &s_readonly_bss_region;
}

const struct pbl_mpu_region *memory_layout_get_worker_region(void) {
  return &s_worker_region;
}

const struct pbl_mpu_region *memory_layout_get_microflash_region(void) {
  return &s_microflash_region;
}

void memory_layout_thread_regions_init(void) {
  // SF32LB52 caches this memory through the background map but not through these
  // inner-shareable regions, so an access without them can leave dirty lines the
  // process never sees, or read stale ones. Every thread has to use them, as ISRs
  // inherit the regions of the thread they interrupt.
  const struct pbl_mpu_region *regions[PBL_THREAD_MAX_MEM_REGIONS] = {
    [MEMORY_LAYOUT_THREAD_REGION_APP] = &s_app_region,
    [MEMORY_LAYOUT_THREAD_REGION_WORKER] = &s_worker_region,
  };
  pbl_thread_regions_default_set(regions);
}

bool memory_layout_is_pointer_in_region(const struct pbl_mpu_region *region, const void *ptr) {
  return pbl_mpu_region_contains(region, ptr);
}

bool memory_layout_is_buffer_in_region(const struct pbl_mpu_region *region, const void *buf,
                                       size_t length) {
  return pbl_mpu_region_contains_buffer(region, buf, length);
}

bool memory_layout_is_cstring_in_region(const struct pbl_mpu_region *region, const char *str,
                                        size_t max_length) {
  return pbl_mpu_region_contains_cstring(region, str, max_length);
}

#ifdef CONFIG_SHELL
static void prv_shell_line(void *ctx, const char *line) {
  pbl_shell_print(ctx, "%s", line);
}

static int prv_cmd_mpu(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_mpu_regions_print(prv_shell_line, (void *)sh);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, mpu, nullptr, "Dump the MPU regions", prv_cmd_mpu, 0, 0);
#endif
