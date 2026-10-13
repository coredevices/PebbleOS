/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <resource/resource.h>

PBL_WEAK ResAppNum app_get_resource_num(void) {
  return 0;
}

PBL_WEAK bool resource_init_app(ResAppNum app_num, const ResourceVersion *version) {
  return true;
}

PBL_WEAK size_t resource_size(ResAppNum app_num, uint32_t id) {
  return 0;
}

PBL_WEAK size_t resource_load_byte_range_system(ResAppNum app_num, uint32_t id,
                                                uint32_t start_offset, uint8_t *data,
                                                size_t num_bytes) {
  return 0;
}

PBL_WEAK bool resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  return true;
}
