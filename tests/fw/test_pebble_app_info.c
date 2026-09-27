/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "process_management/pebble_process_md.h"

void test_pebble_app_info__simple(void) {
  cl_assert(version_compare((Version){5, 1}, (Version){5, 1}) == 0);
  cl_assert(version_compare((Version){5, 2}, (Version){5, 1}) > 0);
  cl_assert(version_compare((Version){5, 0}, (Version){5, 1}) < 0);

  cl_assert(version_compare((Version){4, 2}, (Version){5, 1}) < 0);
  cl_assert(version_compare((Version){6, 0}, (Version){5, 1}) > 0);
}

void test_pebble_app_info__sizes_ignore_high_bytes_before_0x10_01(void) {
  // Older binaries have linker padding where the high bytes now sit
  const PebbleProcessInfo info = {
    .struct_version = {0x10, 0x00},
    .load_size = 0xd78c,
    .virtual_size = 0xeaf3,
    .load_size_hi = 0xaa,
    .virtual_size_hi = 0x55,
  };

  cl_assert_equal_i(process_info_get_load_size(&info), 0xd78c);
  cl_assert_equal_i(process_info_get_virtual_size(&info), 0xeaf3);
}

void test_pebble_app_info__sizes_ignore_high_bytes_in_legacy_headers(void) {
  const PebbleProcessInfo info = {
    .struct_version = {0x08, 0x02},
    .load_size = 0x1234,
    .virtual_size = 0x5678,
    .load_size_hi = 0x01,
    .virtual_size_hi = 0x01,
  };

  cl_assert_equal_i(process_info_get_load_size(&info), 0x1234);
  cl_assert_equal_i(process_info_get_virtual_size(&info), 0x5678);
}

void test_pebble_app_info__sizes_use_high_bytes_from_0x10_01(void) {
  const PebbleProcessInfo info = {
    .struct_version = {0x10, 0x01},
    .load_size = 0x2c00,
    .virtual_size = 0x8000,
    .load_size_hi = 0x01,
    .virtual_size_hi = 0x01,
  };

  cl_assert_equal_i(process_info_get_load_size(&info), 0x12c00);
  cl_assert_equal_i(process_info_get_virtual_size(&info), 0x18000);
}

void test_pebble_app_info__sizes_below_64k_read_the_same_in_0x10_01(void) {
  const PebbleProcessInfo info = {
    .struct_version = {0x10, 0x01},
    .load_size = 0xd78c,
    .virtual_size = 0xeaf3,
  };

  cl_assert_equal_i(process_info_get_load_size(&info), 0xd78c);
  cl_assert_equal_i(process_info_get_virtual_size(&info), 0xeaf3);
}
