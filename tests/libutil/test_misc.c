/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include <pbl/util/misc.h>

struct pair {
  int16_t a;
  int16_t b;
};

void test_misc__swap(void) {
  int16_t x = -1;
  int16_t y = 7;
  PBL_SWAP(x, y);
  cl_assert_equal_i(x, 7);
  cl_assert_equal_i(y, -1);

  struct pair p = {.a = 1, .b = 2};
  PBL_SWAP(p.a, p.b);
  cl_assert_equal_i(p.a, 2);
  cl_assert_equal_i(p.b, 1);
}

void test_misc__fourcc(void) {
  cl_assert_equal_i(PBL_FOURCC('P', 'D', 'C', 'S'), 0x50444353);
  cl_assert_equal_i(PBL_FOURCC('\x89', 'P', 'N', 'G'), 0x89504e47);
}

void test_misc__container_of(void) {
  struct pair p;
  cl_assert(container_of(&p.b, struct pair, b) == &p);
}
