/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/mcu/mpu.h>

#define KERNEL_READONLY_DATA PBL_SECTION(".kernel_unpriv_ro_bss")

enum MemoryRegionAssignments {
// SF32LB52: SiFli's system_bf0_ap.c programs MPU regions 0..4 in
// SystemInit() (flash, peripherals, .ramfunc, LPSYS RAM, HCPU<->LCPU
// mailbox). Reserve four slots here so the per-task configurable
// regions still land at indices 8..11 -- matching the FreeRTOS port's
// FREERTOS_FIRST_MPU_REGION=8. SiFli's fifth region (mailbox at index
// 4) shares its slot with the unused-on-SF32LB52 MemoryRegion_Flash
// value; Pebble never programs region 4, so SiFli's mailbox config
// stays intact.
#ifdef CONFIG_SOC_SF32LB52
  MemoryRegion_Reserved0,
  MemoryRegion_Reserved1,
  MemoryRegion_Reserved2,
  MemoryRegion_Reserved3,
#endif
  MemoryRegion_Flash,
  MemoryRegion_ReadOnlyBss,
  MemoryRegion_ResourceBanks,
  MemoryRegion_IsrStackGuard,
  MemoryRegion_AppRAM,
  MemoryRegion_WorkerRAM,
  MemoryRegion_TaskStackGuard,
  MemoryRegion_Task4
};

//! Index of the stack guard in pbl_thread_attr::regions.
#define MEMORY_LAYOUT_THREAD_REGION_STACK_GUARD (MemoryRegion_TaskStackGuard - MemoryRegion_AppRAM)
//! Index of the process syscall stack guard in pbl_thread_attr::regions.
#define MEMORY_LAYOUT_THREAD_REGION_SYSCALL_GUARD (MemoryRegion_Task4 - MemoryRegion_AppRAM)

void memory_layout_dump_mpu_regions(void);

void memory_layout_setup_mpu(void);

const MpuRegion *memory_layout_get_app_region(void);
const MpuRegion *memory_layout_get_app_stack_guard_region(void);

const MpuRegion *memory_layout_get_readonly_bss_region(void);
const MpuRegion *memory_layout_get_microflash_region(void);

const MpuRegion *memory_layout_get_worker_region(void);
const MpuRegion *memory_layout_get_worker_stack_guard_region(void);

//! Gives every thread that does not set its own the kernel view of App and Worker RAM.
void memory_layout_thread_regions_init(void);

//! Sets up the guard for the stack starting at @p stack, with @c CONFIG_KERNEL_STACK_GUARD_SIZE
//! bytes reserved below it.
//! @return @p guard, or NULL if the target needs no region to catch overflows.
const MpuRegion *memory_layout_stack_guard_region_init(MpuRegion *guard, const void *stack);

bool memory_layout_is_pointer_in_region(const MpuRegion *region, const void *ptr);
bool memory_layout_is_buffer_in_region(const MpuRegion *region, const void *buf, size_t length);
bool memory_layout_is_cstring_in_region(const MpuRegion *region, const char *str,
                                        size_t max_length);
