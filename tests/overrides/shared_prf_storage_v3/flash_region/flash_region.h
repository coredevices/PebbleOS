/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define SECTOR_SIZE_BYTES                           (0x10000)
#define SECTOR_ADDR_MASK                            (~(SECTOR_SIZE_BYTES - 1))

#define SUBSECTOR_SIZE_BYTES                        (0x1000)
#define SUBSECTOR_ADDR_MASK                         (~(SUBSECTOR_SIZE_BYTES - 1))

#define FLASH_REGION_SHARED_PRF_STORAGE_BEGIN 0x0
#define FLASH_REGION_SHARED_PRF_STORAGE_END 0x1000


void flash_region_erase_optimal_range(uint32_t min_start, uint32_t max_start,
                                      uint32_t min_end, uint32_t max_end);

void flash_region_erase_optimal_range_no_watchdog(uint32_t min_start, uint32_t max_start,
                                                  uint32_t min_end, uint32_t max_end);
