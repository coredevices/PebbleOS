/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_regions.h"

#include <pbl/kernel/thread.h>

#if defined(UNITTEST) || defined(CONFIG_ARCH_POSIX)
static const uint32_t __APP_RAM__ = 0;
static const uint32_t __APP_RAM_size__ = 0;
static const uint32_t __WORKER_RAM__ = 0;
static const uint32_t __WORKER_RAM_size__ = 0;

#else
extern const uint32_t __APP_RAM__[];
extern const uint32_t __APP_RAM_size__[];
extern const uint32_t __WORKER_RAM__[];
extern const uint32_t __WORKER_RAM_size__[];

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

const struct pbl_mpu_region *process_region_app(void) {
  return &s_app_region;
}

const struct pbl_mpu_region *process_region_worker(void) {
  return &s_worker_region;
}

void process_regions_thread_init(void) {
  // SF32LB52 caches this memory through the background map but not through these
  // inner-shareable regions, so an access without them can leave dirty lines the
  // process never sees, or read stale ones. Every thread has to use them, as ISRs
  // inherit the regions of the thread they interrupt.
  const struct pbl_mpu_region *regions[PBL_THREAD_MAX_MEM_REGIONS] = {
    [PROCESS_REGIONS_THREAD_APP] = &s_app_region,
    [PROCESS_REGIONS_THREAD_WORKER] = &s_worker_region,
  };
  pbl_thread_regions_default_set(regions);
}
