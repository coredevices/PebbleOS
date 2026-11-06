/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#undef CONFIG_BOARD_PARTITIONS_DUAL_SLOT
#define CONFIG_BOARD_PARTITIONS_SINGLE_SLOT 1

#include <pbl/board/partitions.h>
#include <pbl/util/size.h>

#define LAYOUT_ENTRY(name) {.offset = PART_##name##_OFFSET, .size = PART_##name##_SIZE}

const struct pbl_flash_partition g_single_slot_partitions[] = {
  LAYOUT_ENTRY(SAFE_FIRMWARE),
  LAYOUT_ENTRY(MFG_INFO),
  LAYOUT_ENTRY(FIRMWARE_SLOT_1),
  LAYOUT_ENTRY(SYSTEM_RESOURCES_BANK_0),
  LAYOUT_ENTRY(SYSTEM_RESOURCES_BANK_1),
  LAYOUT_ENTRY(FILESYSTEM),
  LAYOUT_ENTRY(DEBUG_DB),
  LAYOUT_ENTRY(MFG_RESULTS),
  LAYOUT_ENTRY(MFG_BATTERY_STATE),
  LAYOUT_ENTRY(SHARED_PRF_STORAGE),
};
const size_t g_single_slot_partitions_len = ARRAY_LENGTH(g_single_slot_partitions);
