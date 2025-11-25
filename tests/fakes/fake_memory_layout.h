/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/mpu.h"

// Everything but NULL + 256
const MpuRegion s_fake_app_region = { .region_num = 9, .enabled = true, .base_address = 256,
  .size = 0xFFFFFFFF - (1 << 8), .priv_read = true, .priv_write = true, .user_read = true, .user_write = true };

const MpuRegion* memory_layout_get_app_region(void) {
  return &s_fake_app_region;
}

bool memory_layout_is_pointer_in_region(const MpuRegion *region, const void *ptr) {
  uintptr_t p = (uintptr_t) ptr;
  return (p >= region->base_address && p < (region->base_address + region->size));
}

bool memory_layout_is_buffer_in_region(const MpuRegion *region, const void *buf, size_t length) {
  return memory_layout_is_pointer_in_region(region, buf) && memory_layout_is_pointer_in_region(region, buf + length);
}
