/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/mpu.h>
#include <pbl/util/math.h>
#include <pbl/util/string.h>

bool pbl_mpu_region_contains(const struct pbl_mpu_region *region, const void *ptr) {
  const uintptr_t p = (uintptr_t)ptr;
  return p >= region->base && p < region->base + region->size;
}

bool pbl_mpu_region_contains_buffer(const struct pbl_mpu_region *region, const void *buf,
                                    size_t length) {
  uintptr_t buf_end;
  if (ckd_add(&buf_end, (uintptr_t)buf, length)) {
    return false;
  }
  return pbl_mpu_region_contains(region, buf) &&
         pbl_mpu_region_contains(region, (const void *)(buf_end - 1));
}

bool pbl_mpu_region_contains_cstring(const struct pbl_mpu_region *region, const char *str,
                                     size_t max_length) {
  if (!pbl_mpu_region_contains(region, str)) {
    return false;
  }

  const size_t avail = region->base + region->size - (uintptr_t)str;
  const size_t scan_len = (max_length < avail) ? max_length + 1 : avail;
  return strnlen(str, scan_len) < scan_len;
}
