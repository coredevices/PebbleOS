/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include "util/graphics.h"

// Make sure that row stride, bit depth is all being used correctly.
// Really, these are just some simple cases and sanity checks.
void test_graphics__raw_image_get_value_for_bitdepth(void) {
  uint8_t test0[] = { 0b11000000 };
  uint8_t test1[] = { 0, 0, 0, 0b11000000, 0 };
  uint8_t test2[] = { 0b11000000, 0b00110000, 0b00001100, 0b00000011 };

  cl_assert(raw_image_get_value_for_bitdepth(test0, 0, 0, 1, 1) == 1);
  cl_assert(raw_image_get_value_for_bitdepth(test0, 7, 0, 1, 1) == 0);
  cl_assert(raw_image_get_value_for_bitdepth(test0, 0, 0, 1, 2) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test0, 0, 0, 1, 4) == 12);
  cl_assert(raw_image_get_value_for_bitdepth(test0, 0, 0, 1, 8) == 192);
  cl_assert(raw_image_get_value_for_bitdepth(test0, 0, 0, 1, 1) == 1);
  cl_assert(raw_image_get_value_for_bitdepth(test0, 1, 0, 1, 1) == 1);

  cl_assert(raw_image_get_value_for_bitdepth(test1, 0, 3, 1, 2) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test1, 0, 3, 1, 1) == 1);
  cl_assert(raw_image_get_value_for_bitdepth(test1, 0, 3, 1, 4) == 12);
  cl_assert(raw_image_get_value_for_bitdepth(test1, 0, 3, 1, 8) == 192);
  cl_assert(raw_image_get_value_for_bitdepth(test1, 4, 1, 2, 2) == 3);

  cl_assert(raw_image_get_value_for_bitdepth(test2, 5, 2, 1, 1) == 1);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 6, 2, 1, 1) == 0);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 5, 0, 2, 2) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 5, 0, 2, 2) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 1, 1, 2, 8) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 0, 1, 3, 8) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 1, 1, 1, 2) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 0, 1, 1, 4) == 3);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 1, 1, 2, 4) == 12);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 0, 1, 1, 8) == 48);
  cl_assert(raw_image_get_value_for_bitdepth(test2, 3, 0, 4, 8) == 3);
}

