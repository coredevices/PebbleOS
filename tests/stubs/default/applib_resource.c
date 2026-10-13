/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <pbl/kernel/compiler.h>

#include <resource/resource.h>

PBL_WEAK bool applib_resource_track_mmapped(const void *bytes) {
  return false;
}

PBL_WEAK bool applib_resource_is_mmapped(const void *bytes) {
  return false;
}

PBL_WEAK bool applib_resource_munmap(const void *bytes) {
  return false;
}

PBL_WEAK bool applib_resource_munmap_all() {
  return false;
}

PBL_WEAK void applib_resource_munmap_or_free(void *bytes) {
  free(bytes);
}

// this is just a stub, if you need proper resource handling
// link against fake_applib_resource.c in your test
PBL_WEAK void *applib_resource_mmap_or_load(ResAppNum app_num, uint32_t resource_id, size_t offset,
                                            size_t num_bytes, bool used_aligned) {
  return nullptr;
}
