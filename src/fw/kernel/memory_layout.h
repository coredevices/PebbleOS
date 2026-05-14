/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/mpu.h"

#include <stddef.h>

#define KERNEL_READONLY_DATA SECTION(".kernel_unpriv_ro_bss")

enum MemoryRegionAssignments {
  // SF32LB52: SiFli's system_bf0_ap.c programs MPU regions 0..4 in
  // SystemInit() (flash, peripherals, .ramfunc, LPSYS RAM, HCPU<->LCPU
  // mailbox). Reserve four slots here so the per-task configurable
  // regions still land at indices 8..11 -- matching the FreeRTOS port's
  // FREERTOS_FIRST_MPU_REGION=8. SiFli's fifth region (mailbox at index
  // 4) shares its slot with the unused-on-SF32LB52 MemoryRegion_Flash
  // value; Pebble never programs region 4, so SiFli's mailbox config
  // stays intact.
#ifdef MICRO_FAMILY_SF32LB52
  MemoryRegion_Reserved0,
  MemoryRegion_Reserved1,
  MemoryRegion_Reserved2,
  MemoryRegion_Reserved3,
#endif
  MemoryRegion_Flash,
  MemoryRegion_ReadOnlyBss,
  MemoryRegion_ReadOnlyData,
  MemoryRegion_IsrStackGuard,
  MemoryRegion_AppRAM,
  MemoryRegion_WorkerRAM,
  MemoryRegion_TaskStackGuard,
  MemoryRegion_Task4
};

void memory_layout_dump_mpu_regions_to_dbgserial(void);

void memory_layout_setup_mpu(void);

const MpuRegion* memory_layout_get_app_region(void);
const MpuRegion* memory_layout_get_app_stack_guard_region(void);

const MpuRegion* memory_layout_get_readonly_bss_region(void);
const MpuRegion* memory_layout_get_microflash_region(void);

const MpuRegion* memory_layout_get_worker_region(void);
const MpuRegion* memory_layout_get_worker_stack_guard_region(void);

const MpuRegion* memory_layout_get_kernel_main_stack_guard_region(void);
const MpuRegion* memory_layout_get_kernel_bg_stack_guard_region(void);

bool memory_layout_is_pointer_in_region(const MpuRegion *region, const void *ptr);
bool memory_layout_is_buffer_in_region(const MpuRegion *region, const void *buf, size_t length);
bool memory_layout_is_cstring_in_region(const MpuRegion *region, const char *str, size_t max_length);
