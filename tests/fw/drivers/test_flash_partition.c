/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/board/partitions.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_spi_flash.h>

#define FLASH_SIZE (32U * 1024U * 1024U)

extern const struct pbl_flash_partition g_single_slot_partitions[];
extern const size_t g_single_slot_partitions_len;

#define LAYOUT_ENTRY(name) {.offset = PART_##name##_OFFSET, .size = PART_##name##_SIZE}

static const struct pbl_flash_partition s_dual_slot_partitions[] = {
  LAYOUT_ENTRY(PTABLE),
  LAYOUT_ENTRY(BOOTLOADER),
  LAYOUT_ENTRY(FIRMWARE_SLOT_0),
  LAYOUT_ENTRY(FIRMWARE_SLOT_1),
  LAYOUT_ENTRY(SYSTEM_RESOURCES_BANK_0),
  LAYOUT_ENTRY(SYSTEM_RESOURCES_BANK_1),
  LAYOUT_ENTRY(SAFE_FIRMWARE),
  LAYOUT_ENTRY(FILESYSTEM),
  LAYOUT_ENTRY(CD),
  LAYOUT_ENTRY(DEBUG_DB),
  LAYOUT_ENTRY(MFG_RESULTS),
  LAYOUT_ENTRY(MFG_BATTERY_STATE),
  LAYOUT_ENTRY(TZINFO),
  LAYOUT_ENTRY(MFG_INFO),
  LAYOUT_ENTRY(SHARED_PRF_STORAGE),
};

static const struct pbl_flash_partition s_part = {.offset = 0x10000U, .size = 0x2000U};

void test_flash_partition__initialize(void) {
  fake_spi_flash_init(0U, FLASH_SIZE);
}

void test_flash_partition__cleanup(void) {
  fake_spi_flash_cleanup();
}

static void prv_check_layout(const struct pbl_flash_partition *parts, size_t len) {
  uint32_t end = 0U;

  for (size_t i = 0U; i < len; i++) {
    cl_assert((parts[i].offset % SUBSECTOR_SIZE_BYTES) == 0U);
    cl_assert((parts[i].size % SUBSECTOR_SIZE_BYTES) == 0U);
    cl_assert(parts[i].size > 0U);
    cl_assert(parts[i].offset >= end);
    end = parts[i].offset + parts[i].size;
  }

  cl_assert_equal_i(end, FLASH_SIZE);
}

void test_flash_partition__dual_slot_layout(void) {
  prv_check_layout(s_dual_slot_partitions, ARRAY_LENGTH(s_dual_slot_partitions));
}

void test_flash_partition__single_slot_layout(void) {
  prv_check_layout(g_single_slot_partitions, g_single_slot_partitions_len);
}

void test_flash_partition__filesystem_is_sector_aligned(void) {
  static_assert((PART_FILESYSTEM_OFFSET % SECTOR_SIZE_BYTES) == 0U);
  static_assert((PART_FILESYSTEM_SIZE % SECTOR_SIZE_BYTES) == 0U);
}

void test_flash_partition__offsets_start_at_the_partition(void) {
  const uint8_t data[] = {1U, 2U, 3U, 4U};
  uint8_t buf[sizeof(data)];

  cl_assert_equal_i(pbl_flash_partition_erase(s_part, 0U, s_part.size), 0);
  cl_assert_equal_i(pbl_flash_partition_write(s_part, 0x10U, data, sizeof(data)), 0);

  cl_assert_equal_i(pbl_flash_read(FLASH, s_part.offset + 0x10U, buf, sizeof(buf)), 0);
  cl_assert_equal_m(buf, data, sizeof(data));

  memset(buf, 0, sizeof(buf));
  cl_assert_equal_i(pbl_flash_partition_read(s_part, 0x10U, buf, sizeof(buf)), 0);
  cl_assert_equal_m(buf, data, sizeof(data));
}

void test_flash_partition__rejects_ranges_outside(void) {
  uint8_t buf[4] = {};

  cl_assert_equal_i(pbl_flash_partition_read(s_part, s_part.size, buf, 1U), -EINVAL);
  cl_assert_equal_i(pbl_flash_partition_read(s_part, s_part.size - 1U, buf, 2U), -EINVAL);
  cl_assert_equal_i(pbl_flash_partition_read(s_part, UINT32_MAX, buf, 2U), -EINVAL);
  cl_assert_equal_i(pbl_flash_partition_write(s_part, s_part.size - 1U, buf, 2U), -EINVAL);
  cl_assert_equal_i(pbl_flash_partition_erase(s_part, 0U, s_part.size + 1U), -EINVAL);
  cl_assert_equal_i(pbl_flash_partition_erase(s_part, s_part.size, SUBSECTOR_SIZE_BYTES), -EINVAL);
  cl_assert(!pbl_flash_partition_is_erased(s_part, 0U, s_part.size + 1U));
}

void test_flash_partition__last_bytes_are_inside(void) {
  const uint8_t data[] = {5U, 6U};
  uint8_t buf[sizeof(data)];

  cl_assert_equal_i(pbl_flash_partition_erase(s_part, 0U, s_part.size), 0);
  cl_assert(pbl_flash_partition_is_erased(s_part, 0U, s_part.size));

  cl_assert_equal_i(pbl_flash_partition_write(s_part, s_part.size - 2U, data, sizeof(data)), 0);
  cl_assert_equal_i(pbl_flash_partition_read(s_part, s_part.size - 2U, buf, sizeof(buf)), 0);
  cl_assert_equal_m(buf, data, sizeof(data));
  cl_assert(!pbl_flash_partition_is_erased(s_part, 0U, s_part.size));
}
