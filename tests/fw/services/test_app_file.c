/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "services/normal/filesystem/app_file.h"

#include "stubs_passert.h"

static void assert_file_name(const char *vector,
                             AppInstallId app_id, const char *suffix) {
  char buf[42];
  sprintf(buf, "@%08"PRIx32"/%s", (uint32_t)app_id, suffix);
  cl_assert_equal_s(vector, buf);
}

// Tests for app_file_name_make()

static void assert_app_file_name_make(AppInstallId app_id, const char *suffix) {
  char buf[42];
  memset(buf, 'X', sizeof(buf));
  buf[41] = '\0';
  app_file_name_make(buf, sizeof(buf), app_id, suffix, strlen(suffix));
  assert_file_name(buf, app_id, suffix);
}

void test_app_file__name_make_simple_1(void) {
  assert_app_file_name_make(1, "app");
}

void test_app_file__name_make_simple_2(void) {
  assert_app_file_name_make(7, "app_app");
}

void test_app_file__name_make_hex_1(void) {
  assert_app_file_name_make(0x5abc, "woop");
}

void test_app_file__name_make_hex_2(void) {
  assert_app_file_name_make(0x12345, "looks_like_decimal");
}

void test_app_file__name_make_negative(void) {
  assert_app_file_name_make(-42, "builtin_app");
}

void test_app_file__name_make_pseudo_directory(void) {
  assert_app_file_name_make(76, "not/really/a/path");;
}

void test_app_file__name_make_no_suffix(void) {
  assert_app_file_name_make(54321, "");;
}

void test_app_file__name_make_buf_just_big_enough(void) {
  char buf[12];
  app_file_name_make(buf, sizeof(buf), 123, "a", 1);
}

void test_app_file__name_make_buf_too_small_for_suffix(void) {
  char buf[12];
  cl_assert_passert(app_file_name_make(buf, sizeof(buf), 123, "ab", 2));
}

void test_app_file__name_make_buf_too_small_for_prefix(void) {
  char buf[10];
  cl_assert_passert(app_file_name_make(buf, sizeof(buf), 123, "", 0));
}

// Tests for is_app_file_name()

void test_app_file__is_app_file_name_simple(void) {
  cl_assert(is_app_file_name("@00000001/abc") == true);
}

void test_app_file__is_app_file_name_hex(void) {
  cl_assert(is_app_file_name("@abcdef01/abc") == true);
}

void test_app_file__is_app_file_name_negative(void) {
  cl_assert(is_app_file_name("@feedface/abc") == true);
}

void test_app_file__is_app_file_name_obviously_false(void) {
  cl_assert(is_app_file_name("appdb") == false);
}

void test_app_file__is_app_file_name_tricky_false_1(void) {
  cl_assert(is_app_file_name("@1234567/abc") == false);
}

void test_app_file__is_app_file_name_tricky_false_2(void) {
  cl_assert(is_app_file_name("12345678/abc") == false);
}

void test_app_file__is_app_file_name_tricky_false_3(void) {
  cl_assert(is_app_file_name("@12345678\\foo") == false);
}

void test_app_file__is_app_file_name_tricky_false_4(void) {
  cl_assert(is_app_file_name("@abcdefg1/def") == false);
}

void test_app_file__is_app_file_name_tricky_false_5(void) {
  cl_assert(is_app_file_name("?01234567/ghi") == false);
}

void test_app_file__is_app_file_name_tricky_false_6(void) {
  cl_assert(is_app_file_name("A12345678/jkl") == false);
}

void test_app_file__is_app_file_name_tricky_false_7(void) {
  cl_assert(is_app_file_name("@12345678.foo") == false);
}

void test_app_file__is_app_file_name_tricky_false_8(void) {
  cl_assert(is_app_file_name("@123456780bar") == false);
}

// Tests for app_file_get_app_id()

void test_app_file__get_app_id_simple(void) {
  cl_assert_equal_i(app_file_get_app_id("@00000001/abc"), 1);
}

void test_app_file__get_app_id_invalid_1(void) {
  cl_assert_equal_i(app_file_get_app_id("pindb"), INSTALL_ID_INVALID);
}

void test_app_file__get_app_id_invalid_2(void) {
  cl_assert_equal_i(app_file_get_app_id("@abcdefg0/foo"), INSTALL_ID_INVALID);
}

void test_app_file__get_app_id_hex(void) {
  cl_assert_equal_i(app_file_get_app_id("@01cba987/nums"), 0x01cba987);
}

void test_app_file__get_app_id_negative(void) {
  cl_assert_equal_i(app_file_get_app_id("@ffffffe9/asdf"), -23);
}
