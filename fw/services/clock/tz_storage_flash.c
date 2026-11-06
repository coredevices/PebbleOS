/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "tz_storage.h"

#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash.h>
#include <pbl/kernel/compiler.h>

// Versioned so that the layout can be migrated.
typedef struct PBL_PACKED {
  uint8_t version;
  char tm_zone[PBL_TZ_LEN - 1];
  uint8_t dst_id;
  int16_t timezone_id;
  int32_t tm_gmtoff;
  time_t dst_start;
  time_t dst_end;
} TzinfoFlashStorage;

#define TZINFO_VERSION 1

static_assert(sizeof(TzinfoFlashStorage) <= PART_TZINFO_SIZE,
              "TzinfoFlashStorage must fit in the TZINFO flash region");

void tz_storage_save(const TimezoneInfo *tz_info) {
  TzinfoFlashStorage storage = {
    .version = TZINFO_VERSION,
    .dst_id = tz_info->dst_id,
    .timezone_id = tz_info->timezone_id,
    .tm_gmtoff = tz_info->tm_gmtoff,
    .dst_start = tz_info->dst_start,
    .dst_end = tz_info->dst_end,
  };
  memcpy(storage.tm_zone, tz_info->tm_zone, PBL_TZ_LEN - 1);

  pbl_flash_partition_erase(PART_TZINFO, 0U, PART_TZINFO_SIZE);
  pbl_flash_partition_write(PART_TZINFO, 0U, &storage, sizeof(storage));
}

void tz_storage_load(TimezoneInfo *tz_info) {
  TzinfoFlashStorage storage;

  pbl_flash_partition_read(PART_TZINFO, 0U, &storage, sizeof(storage));
  if (storage.version != TZINFO_VERSION) {
    memset(tz_info, 0, sizeof(*tz_info));
    return;
  }

  memcpy(tz_info->tm_zone, storage.tm_zone, PBL_TZ_LEN - 1);
  tz_info->dst_id = storage.dst_id;
  tz_info->timezone_id = storage.timezone_id;
  tz_info->tm_gmtoff = storage.tm_gmtoff;
  tz_info->dst_start = storage.dst_start;
  tz_info->dst_end = storage.dst_end;
}

bool tz_storage_is_set(void) {
  uint8_t version;

  pbl_flash_partition_read(PART_TZINFO, 0U, &version, sizeof(version));

  return version == TZINFO_VERSION;
}

void tz_storage_clear(void) {
  pbl_flash_partition_erase(PART_TZINFO, 0U, PART_TZINFO_SIZE);
}
