/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/util/ratio.h"

void test_ratio__to_percent(void) {
  cl_assert_equal_i(pbl_ratio32_to_percent(0), 0);
  cl_assert_equal_i(pbl_ratio32_to_percent(PBL_RATIO32_MAX / 2), 49);
  cl_assert_equal_i(pbl_ratio32_to_percent(PBL_RATIO32_MAX), 100);
}

void test_ratio__from_percent(void) {
  cl_assert_equal_i(pbl_ratio32_from_percent(0), 1);
  cl_assert_equal_i(pbl_ratio32_from_percent(50), 32768);
  cl_assert_equal_i(pbl_ratio32_from_percent(100), PBL_RATIO32_MAX + 1);
}

void test_ratio__round_trip(void) {
  for (uint32_t percent = 0; percent <= 100; percent++) {
    cl_assert_equal_i(pbl_ratio32_to_percent(pbl_ratio32_from_percent(percent)), percent);
  }
}
