/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/mpu.h>

// Everything but NULL + 256
const struct pbl_mpu_region s_fake_app_region = {
  .base = 256,
  .size = 0xFFFFFFFF - (1 << 8),
  .attr = PBL_MPU_P_RW_U_RW,
};

const struct pbl_mpu_region *memory_layout_get_app_region(void) {
  return &s_fake_app_region;
}

bool memory_layout_is_pointer_in_region(const struct pbl_mpu_region *region, const void *ptr) {
  uintptr_t p = (uintptr_t)ptr;
  return (p >= region->base && p < (region->base + region->size));
}

bool memory_layout_is_buffer_in_region(const struct pbl_mpu_region *region, const void *buf, size_t length) {
  return memory_layout_is_pointer_in_region(region, buf) &&
         memory_layout_is_pointer_in_region(region, buf + length);
}
