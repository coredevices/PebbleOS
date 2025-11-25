/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/applib_resource_private.h"
#include "fake_resource_syscalls.h"

bool applib_resource_track_mmapped(const void *bytes) {
  return false;
}

bool applib_resource_is_mmapped(const void *bytes) {
  return false;
}

bool applib_resource_munmap(const void *bytes) {
  return false;
}

bool applib_resource_munmap_all() {
  return false;
}

void applib_resource_munmap_or_free(void *bytes) {
  free(bytes);
}

void *applib_resource_mmap_or_load(ResAppNum app_num, uint32_t resource_id,
                                   size_t offset, size_t num_bytes, bool used_aligned) {
  uint8_t *result = malloc(num_bytes + (used_aligned ? 7 : 0));
  if (!result || sys_resource_load_range(app_num, resource_id, offset,
                                         result, num_bytes) != num_bytes) {
    free(result);
    return NULL;
  }
  return result;
}

