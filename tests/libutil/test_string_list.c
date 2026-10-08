/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/string_list.h>

#include <clar.h>

// Setup
////////////////////////////////////////////////////////////////

void test_string_list__initialize(void) {
}

void test_string_list__cleanup(void) {
}

// Tests
////////////////////////////////////////////////////////////////

void test_string_list__test(void) {
  struct pbl_string_list *list = malloc(20);
  memset(list, 0, 20);
  // no data
  list->serialized_byte_length = 0;
  cl_assert_equal_i(0, pbl_string_list_count(list));

  list->serialized_byte_length = 3;
  // 4 empty strings
  cl_assert_equal_i(4, pbl_string_list_count(list));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 0));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 1));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 2));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 3));

  // non-null-terminated string is treated as one string - this is the standard case
  // please note that the string will only be terminated if there's another \0 following
  // when deserializing the data, the deserializer will append the needed \0
  list->serialized_byte_length = 3;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = 'c'; // end of data
  list->data[3] = 'd';
  list->data[4] = '\0';
  cl_assert_equal_i(1, pbl_string_list_count(list));
  cl_assert_equal_s("abcd", pbl_string_list_get_at(list, 0));

  // 1 string (null terminated) => 2 strings, last is empty
  list->serialized_byte_length = 3;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = '\0'; // end of data
  list->data[3] = '\0';
  cl_assert_equal_i(2, pbl_string_list_count(list));
  cl_assert_equal_s("ab", pbl_string_list_get_at(list, 0));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 1));

  // 2 strings (non-null terminated) - this is the standard case
  list->serialized_byte_length = 4;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = '\0';
  list->data[3] = 'c'; // end of data
  list->data[4] = '\0';
  cl_assert_equal_i(2, pbl_string_list_count(list));
  cl_assert_equal_s("ab", pbl_string_list_get_at(list, 0));
  cl_assert_equal_s("c", pbl_string_list_get_at(list, 1));

  // 3 strings (last two are is empty)
  list->serialized_byte_length = 4;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = '\0';
  list->data[3] = '\0'; // end of data
  list->data[4] = '\0';
  cl_assert_equal_i(3, pbl_string_list_count(list));
  cl_assert_equal_s("ab", pbl_string_list_get_at(list, 0));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 1));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 2));
  cl_assert_equal_s(NULL, pbl_string_list_get_at(list, 3));

  // 4 strings (first and last two are empty)
  list->serialized_byte_length = 4;
  list->data[0] = '\0';
  list->data[1] = 'b';
  list->data[2] = '\0';
  list->data[3] = '\0'; // end of data
  list->data[4] = '\0';
  cl_assert_equal_i(4, pbl_string_list_count(list));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 0));
  cl_assert_equal_s("b", pbl_string_list_get_at(list, 1));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 2));
  cl_assert_equal_s("", pbl_string_list_get_at(list, 3));

  // 2 strings (last is not terminated and will fall through) will return 2 strings
  // when deserializing, the deserializer puts a \0 at the end
  // this case demonstrates the problem with incorrectly initialized data
  list->serialized_byte_length = 3;
  list->data[0] = 'a';
  list->data[1] = '\0';
  list->data[2] = 'b'; // end of data
  list->data[3] = 'c';
  list->data[4] = '\0';
  cl_assert_equal_i(2, pbl_string_list_count(list));
  cl_assert_equal_s("a", pbl_string_list_get_at(list, 0));
  cl_assert_equal_s("bc", pbl_string_list_get_at(list, 1));

  // add a string to an empty string list
  list->serialized_byte_length = 0;
  pbl_string_list_add_string(list, 20, "hello", 10);
  cl_assert_equal_i(5, list->serialized_byte_length);
  cl_assert_equal_i(1, pbl_string_list_count(list));
  cl_assert_equal_s("hello", pbl_string_list_get_at(list, 0));

  // add a string to string list with strings
  pbl_string_list_add_string(list, 20, "world", 10);
  cl_assert_equal_i(11, list->serialized_byte_length);
  cl_assert_equal_i(2, pbl_string_list_count(list));
  cl_assert_equal_s("world", pbl_string_list_get_at(list, 1));

  // truncated because of the max string size
  pbl_string_list_add_string(list, 20, "foobar", 3);
  cl_assert_equal_i(15, list->serialized_byte_length);
  cl_assert_equal_i(3, pbl_string_list_count(list));
  cl_assert_equal_s("foo", pbl_string_list_get_at(list, 2));

  // truncated because of the max list size
  pbl_string_list_add_string(list, 20, "abc", 10);
  cl_assert_equal_i(17, list->serialized_byte_length);
  cl_assert_equal_i(4, pbl_string_list_count(list));
  cl_assert_equal_s("a", pbl_string_list_get_at(list, 3));
}
