/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource/resource_storage_flash.h"
#include "resource/resource_storage_impl.h"
#include "resource/resource.h"

#include <stdio.h>
#include <inttypes.h>

#include "flash_region/flash_region.h"
#include "util/string.h"

void resource_storage_get_file_name(char *name, size_t buf_length, ResAppNum resource_bank) {
  concat_str_int("res_bank", resource_bank, name, buf_length);
}

void resource_storage_clear(ResAppNum app_num) {
  return;
}

const SystemResourceBank * resource_storage_flash_get_unused_bank(void) {
  static const SystemResourceBank unused_bank = {
    .begin = FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN,
    .end = FLASH_REGION_SYSTEM_RESOURCES_BANK_1_END,
  };
  return &unused_bank;
}
