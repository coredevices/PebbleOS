/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "clar.h"

// "Define" libc functions we're testing
#include "pblibc_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Tests

void test_strtol__basic(void) {
  cl_assert_equal_i(strtol("500", NULL, 10), 500);
  cl_assert_equal_i(strtol("765", NULL, 10), 765);
  cl_assert_equal_i(strtol("573888", NULL, 10), 573888);
  cl_assert_equal_i(strtol("713713", NULL, 10), 713713);

  cl_assert_equal_i(strtol("2147483646", NULL, 10), 2147483646);
  cl_assert_equal_i(strtol("-2147483647", NULL, 10), -2147483647);
}

void test_strtol__whitespace_pfx(void) {
  cl_assert_equal_i(strtol("     500", NULL, 10), 500);
  cl_assert_equal_i(strtol(" 765", NULL, 10), 765);
  cl_assert_equal_i(strtol("                 573888", NULL, 10), 573888);
  cl_assert_equal_i(strtol("        713713", NULL, 10), 713713);
}

void test_strtol__suffix(void) {
  cl_assert_equal_i(strtol("500hurf", NULL, 10), 500);
  cl_assert_equal_i(strtol("765berserker", NULL, 10), 765);
  cl_assert_equal_i(strtol("573888 redmage", NULL, 10), 573888);
  cl_assert_equal_i(strtol("713713 4 job fiesta111", NULL, 10), 713713);
}

void test_strtol__sign(void) {
  cl_assert_equal_i(strtol("+500", NULL, 10), 500);
  cl_assert_equal_i(strtol("-765", NULL, 10), -765);
  cl_assert_equal_i(strtol("   -573888", NULL, 10), -573888);
  cl_assert_equal_i(strtol("  +713713", NULL, 10), +713713);
}

void test_strtol__error(void) {
  cl_assert_equal_i(strtol("2147483647", NULL, 10), 2147483647); // last valid value
  cl_assert_equal_i(strtol("-2147483648", NULL, 10), -2147483648); // last valid value
  cl_assert_equal_i(strtol("3294967287", NULL, 10), INT_MAX); // signed integer overflow
  cl_assert_equal_i(strtol("2147483648", NULL, 10), INT_MAX);
  cl_assert_equal_i(strtol("-2147483649", NULL, 10), INT_MIN);
}

static const struct {
  intmax_t value;
  int base;
  const char *str;
} s_base_test_data[] = {
  { 2147483646LL, 2, "1111111111111111111111111111110", },
  { 2147483646LL, 2, "1111111111111111111111111111110", },
  { -2147483647LL, 2, "-1111111111111111111111111111111", },
  { -2147483647LL, 2, "-1111111111111111111111111111111", },
  { 2147483646LL, 3, "12112122212110202100", },
  { 2147483646LL, 3, "12112122212110202100", },
  { -2147483647LL, 3, "-12112122212110202101", },
  { -2147483647LL, 3, "-12112122212110202101", },
  { 2147483646LL, 4, "1333333333333332", },
  { 2147483646LL, 4, "1333333333333332", },
  { -2147483647LL, 4, "-1333333333333333", },
  { -2147483647LL, 4, "-1333333333333333", },
  { 2147483646LL, 5, "13344223434041", },
  { 2147483646LL, 5, "13344223434041", },
  { -2147483647LL, 5, "-13344223434042", },
  { -2147483647LL, 5, "-13344223434042", },
  { 2147483646LL, 6, "553032005530", },
  { 2147483646LL, 6, "553032005530", },
  { -2147483647LL, 6, "-553032005531", },
  { -2147483647LL, 6, "-553032005531", },
  { 2147483646LL, 7, "104134211160", },
  { 2147483646LL, 7, "104134211160", },
  { -2147483647LL, 7, "-104134211161", },
  { -2147483647LL, 7, "-104134211161", },
  { 2147483646LL, 8, "17777777776", },
  { 2147483646LL, 8, "17777777776", },
  { -2147483647LL, 8, "-17777777777", },
  { -2147483647LL, 8, "-17777777777", },
  { 2147483646LL, 9, "5478773670", },
  { 2147483646LL, 9, "5478773670", },
  { -2147483647LL, 9, "-5478773671", },
  { -2147483647LL, 9, "-5478773671", },
  { 2147483646LL, 10, "2147483646", },
  { 2147483646LL, 10, "2147483646", },
  { -2147483647LL, 10, "-2147483647", },
  { -2147483647LL, 10, "-2147483647", },
  { 2147483646LL, 11, "A02220280", },
  { 2147483646LL, 11, "a02220280", },
  { -2147483647LL, 11, "-A02220281", },
  { -2147483647LL, 11, "-a02220281", },
  { 2147483646LL, 12, "4BB2308A6", },
  { 2147483646LL, 12, "4bb2308a6", },
  { -2147483647LL, 12, "-4BB2308A7", },
  { -2147483647LL, 12, "-4bb2308a7", },
  { 2147483646LL, 13, "282BA4AA9", },
  { 2147483646LL, 13, "282ba4aa9", },
  { -2147483647LL, 13, "-282BA4AAA", },
  { -2147483647LL, 13, "-282ba4aaa", },
  { 2147483646LL, 14, "1652CA930", },
  { 2147483646LL, 14, "1652ca930", },
  { -2147483647LL, 14, "-1652CA931", },
  { -2147483647LL, 14, "-1652ca931", },
  { 2147483646LL, 15, "C87E66B6", },
  { 2147483646LL, 15, "c87e66b6", },
  { -2147483647LL, 15, "-C87E66B7", },
  { -2147483647LL, 15, "-c87e66b7", },
  { 2147483646LL, 16, "7FFFFFFE", },
  { 2147483646LL, 16, "7ffffffe", },
  { -2147483647LL, 16, "-7FFFFFFF", },
  { -2147483647LL, 16, "-7fffffff", },
  { 2147483646LL, 17, "53G7F547", },
  { 2147483646LL, 17, "53g7f547", },
  { -2147483647LL, 17, "-53G7F548", },
  { -2147483647LL, 17, "-53g7f548", },
  { 2147483646LL, 18, "3928G3H0", },
  { 2147483646LL, 18, "3928g3h0", },
  { -2147483647LL, 18, "-3928G3H1", },
  { -2147483647LL, 18, "-3928g3h1", },
  { 2147483646LL, 19, "27C57H31", },
  { 2147483646LL, 19, "27c57h31", },
  { -2147483647LL, 19, "-27C57H32", },
  { -2147483647LL, 19, "-27c57h32", },
  { 2147483646LL, 20, "1DB1F926", },
  { 2147483646LL, 20, "1db1f926", },
  { -2147483647LL, 20, "-1DB1F927", },
  { -2147483647LL, 20, "-1db1f927", },
  { 2147483646LL, 21, "140H2D90", },
  { 2147483646LL, 21, "140h2d90", },
  { -2147483647LL, 21, "-140H2D91", },
  { -2147483647LL, 21, "-140h2d91", },
  { 2147483646LL, 22, "IKF5BF0", },
  { 2147483646LL, 22, "ikf5bf0", },
  { -2147483647LL, 22, "-IKF5BF1", },
  { -2147483647LL, 22, "-ikf5bf1", },
  { 2147483646LL, 23, "EBELF94", },
  { 2147483646LL, 23, "ebelf94", },
  { -2147483647LL, 23, "-EBELF95", },
  { -2147483647LL, 23, "-ebelf95", },
  { 2147483646LL, 24, "B5GGE56", },
  { 2147483646LL, 24, "b5gge56", },
  { -2147483647LL, 24, "-B5GGE57", },
  { -2147483647LL, 24, "-b5gge57", },
  { 2147483646LL, 25, "8JMDNKL", },
  { 2147483646LL, 25, "8jmdnkl", },
  { -2147483647LL, 25, "-8JMDNKM", },
  { -2147483647LL, 25, "-8jmdnkm", },
  { 2147483646LL, 26, "6OJ8IOM", },
  { 2147483646LL, 26, "6oj8iom", },
  { -2147483647LL, 26, "-6OJ8ION", },
  { -2147483647LL, 26, "-6oj8ion", },
  { 2147483646LL, 27, "5EHNCK9", },
  { 2147483646LL, 27, "5ehnck9", },
  { -2147483647LL, 27, "-5EHNCKA", },
  { -2147483647LL, 27, "-5ehncka", },
  { 2147483646LL, 28, "4CLM98E", },
  { 2147483646LL, 28, "4clm98e", },
  { -2147483647LL, 28, "-4CLM98F", },
  { -2147483647LL, 28, "-4clm98f", },
  { 2147483646LL, 29, "3HK7986", },
  { 2147483646LL, 29, "3hk7986", },
  { -2147483647LL, 29, "-3HK7987", },
  { -2147483647LL, 29, "-3hk7987", },
  { 2147483646LL, 30, "2SB6CS6", },
  { 2147483646LL, 30, "2sb6cs6", },
  { -2147483647LL, 30, "-2SB6CS7", },
  { -2147483647LL, 30, "-2sb6cs7", },
  { 2147483646LL, 31, "2D09UC0", },
  { 2147483646LL, 31, "2d09uc0", },
  { -2147483647LL, 31, "-2D09UC1", },
  { -2147483647LL, 31, "-2d09uc1", },
  { 2147483646LL, 32, "1VVVVVU", },
  { 2147483646LL, 32, "1vvvvvu", },
  { -2147483647LL, 32, "-1VVVVVV", },
  { -2147483647LL, 32, "-1vvvvvv", },
  { 2147483646LL, 33, "1LSQTL0", },
  { 2147483646LL, 33, "1lsqtl0", },
  { -2147483647LL, 33, "-1LSQTL1", },
  { -2147483647LL, 33, "-1lsqtl1", },
  { 2147483646LL, 34, "1D8XQRO", },
  { 2147483646LL, 34, "1d8xqro", },
  { -2147483647LL, 34, "-1D8XQRP", },
  { -2147483647LL, 34, "-1d8xqrp", },
  { 2147483646LL, 35, "15V22UL", },
  { 2147483646LL, 35, "15v22ul", },
  { -2147483647LL, 35, "-15V22UM", },
  { -2147483647LL, 35, "-15v22um", },
  { 2147483646LL, 36, "ZIK0ZI", },
  { 2147483646LL, 36, "zik0zi", },
  { -2147483647LL, 36, "-ZIK0ZJ", },
  { -2147483647LL, 36, "-zik0zj", },
  { 0,0,"" },
};

void test_strtol__altbase(void) {
  for(int i = 0; s_base_test_data[i].base != 0; i++) {
    cl_assert_equal_i(strtol(s_base_test_data[i].str, NULL, s_base_test_data[i].base),
                      s_base_test_data[i].value);
  }
}

void test_strtol__zerobase(void) {
  cl_assert_equal_i(strtol("573bb", NULL, 0), 573);
  cl_assert_equal_i(strtol("0x573", NULL, 0), 0x573);
  cl_assert_equal_i(strtol("0573", NULL, 0), 0573);
  cl_assert_equal_i(strtol("   +573bb", NULL, 0), 573);
  cl_assert_equal_i(strtol(" +0x573ghghghgh", NULL, 0), 0x573);
  cl_assert_equal_i(strtol("  +0573faf", NULL, 0), 0573);
  cl_assert_equal_i(strtol("   -573bb", NULL, 0), -573);
  cl_assert_equal_i(strtol(" -0x573ghghghgh", NULL, 0), -0x573);
  cl_assert_equal_i(strtol("  -0573faf", NULL, 0), -0573);
}

void test_strtol__bogus(void) {
  cl_assert_equal_i(strtol(" ", NULL, 10), 0);
  cl_assert_equal_i(strtol(" -", NULL, 10), 0);
  cl_assert_equal_i(strtol("-", NULL, 10), 0);
  cl_assert_equal_i(strtol(" +", NULL, 10), 0);
  cl_assert_equal_i(strtol("+", NULL, 10), 0);
  cl_assert_equal_i(strtol(" -+123", NULL, 10), 0);
  cl_assert_equal_i(strtol("+-123", NULL, 10), 0);
}

void test_strtol__end(void) {
  char *end;
  char *s;

  s = "";
  strtol(s, &end, 10);
  cl_assert_equal_p(s, end);
  cl_assert_equal_i('\0', *end);

  s = "123";
  strtol(s, &end, 10);
  cl_assert_equal_p(s+3, end);
  cl_assert_equal_i('\0', *end);

  s = "123a";
  strtol(s, &end, 10);
  cl_assert_equal_p(s+3, end);
  cl_assert_equal_i('a', *end);

  s = "a123";
  strtol(s, &end, 10);
  cl_assert_equal_p(s, end);
  cl_assert_equal_i('a', *end);
}
