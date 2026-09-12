/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_region/filesystem_regions.h"
#include <pbl/drivers/flash.h>

#include "pbl/util/size.h"

void filesystem_regions_erase_all(void) {
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_region_list); i++) {
    pbl_flash_erase(FLASH, s_region_list[i].start,
                    s_region_list[i].end - s_region_list[i].start);
  }
}
