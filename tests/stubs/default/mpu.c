/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <kernel/memory_layout.h>

PBL_WEAK const uint32_t __app_stack_start__;

PBL_WEAK const uint32_t __app_stack_end__;

PBL_WEAK const uint32_t __isr_stack_start__;

PBL_WEAK const uint32_t __kernel_bg_stack_start__;

PBL_WEAK const uint32_t __kernel_main_stack_start__;

PBL_WEAK const uint32_t __stack_guard_size__;

PBL_WEAK const uint32_t __worker_stack_start__;

PBL_WEAK const uint32_t __privileged_functions_start__;

PBL_WEAK const uint32_t __privileged_functions_size__;

PBL_WEAK const uint32_t __unpriv_ro_bss_start__;

PBL_WEAK const uint32_t __unpriv_ro_bss_size__;

PBL_WEAK const uint32_t __unpriv_ro_data_start__;

PBL_WEAK const uint32_t __unpriv_ro_data_size__;

PBL_WEAK const uint32_t __APP_RAM_start__;

PBL_WEAK const uint32_t __APP_RAM_size__;

PBL_WEAK const uint32_t __WORKER_RAM_start__;

PBL_WEAK const uint32_t __WORKER_RAM_size__;

PBL_WEAK const uint32_t __FLASH_start__;

PBL_WEAK const uint32_t __FLASH_size__;

PBL_WEAK void mpu_enable(void) {
}

PBL_WEAK MpuRegion mpu_get_region(int region_num) {
  return (MpuRegion){};
}

PBL_WEAK void mpu_set_region(const MpuRegion *region) {
}
