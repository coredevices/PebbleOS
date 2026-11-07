/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mpu.h>

PBL_WEAK const uint32_t __app_stack_start__;

PBL_WEAK const uint32_t __app_stack_end__;

PBL_WEAK const uint32_t __isr_stack_start__;

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

PBL_WEAK int pbl_mpu_region_add(const struct pbl_mpu_region *region) {
  return 0;
}

PBL_WEAK bool pbl_mpu_region_is_stack_guard(uintptr_t start, uintptr_t end) {
  return false;
}

PBL_WEAK void pbl_mpu_user_ro_write(void *dst, const void *src, size_t n) {
  memcpy(dst, src, n);
}

PBL_WEAK const struct pbl_mpu_region *pbl_mpu_region_firmware(void) {
  return nullptr;
}

PBL_WEAK bool pbl_mpu_region_contains(const struct pbl_mpu_region *region, const void *ptr) {
  return false;
}

PBL_WEAK bool pbl_mpu_region_contains_buffer(const struct pbl_mpu_region *region, const void *buf,
                                             size_t length) {
  return false;
}

PBL_WEAK bool pbl_mpu_region_contains_cstring(const struct pbl_mpu_region *region, const char *str,
                                              size_t max_length) {
  return false;
}

PBL_WEAK void pbl_mpu_regions_print(void (*print)(void *ctx, const char *line), void *ctx) {
}
