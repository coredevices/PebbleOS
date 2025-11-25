/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <util/size.h>
#include <util/sort.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static int prv_cmp(int32_t a, int32_t b) {
  if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  } else {
    return 0;
  }
}

static int prv_uint8_cmp(const void *a, const void *b) {
  uint8_t t_a = *(uint8_t *)a;
  uint8_t t_b = *(uint8_t *)b;
  return prv_cmp(t_a, t_b);
}

static int prv_int32_cmp(const void *a, const void *b) {
  int32_t t_a = *(int32_t *)a;
  int32_t t_b = *(int32_t *)b;
  return prv_cmp(t_a, t_b);
}

static int prv_int32_cmp_desc(const void *a, const void *b) {
  return -prv_int32_cmp(a, b);
}

void test_sort__uint8_array(void) {
  uint8_t array[] = {9, 1, 8, 2, 7, 3, 6, 4, 6, 5, 5};

  sort_bubble(array, ARRAY_LENGTH(array), sizeof(uint8_t), prv_uint8_cmp);

  uint8_t sorted[] = {1, 2, 3, 4, 5, 5, 6, 6, 7, 8, 9};
  cl_assert_equal_m(array, sorted, sizeof(array));
}

void test_sort__int32_array(void) {
  int32_t array[] = {-9, 1, 8, 2, 7, 3, -6, 4, 6, 5, 5};

  sort_bubble(array, ARRAY_LENGTH(array), sizeof(int32_t), prv_int32_cmp);

  int32_t sorted[] = {-9, -6, 1, 2, 3, 4, 5, 5, 6, 7, 8};
  cl_assert_equal_m(array, sorted, sizeof(array));
}

void test_sort__int32_array_desc(void) {
  int32_t array[] = {-9, 1, 8, 2, 7, 3, -6, 4, 6, 5, 5};

  sort_bubble(array, ARRAY_LENGTH(array), sizeof(int32_t), prv_int32_cmp_desc);

  int32_t sorted[] = {8, 7, 6, 5, 5, 4, 3, 2, 1, -6, -9};
  cl_assert_equal_m(array, sorted, sizeof(array));
}

void test_sort__single_element_array(void) {
  int32_t array[] = {1};

  sort_bubble(array, ARRAY_LENGTH(array), sizeof(int32_t), prv_int32_cmp);

  int32_t sorted[] = {1};
  cl_assert_equal_m(array, sorted, sizeof(array));
}

typedef struct MyStruct {
  uint8_t nothing;
  int32_t number;
  uint16_t nothing2;
} MyStruct;

static int prv_MyStruct_cmp(const void *a, const void *b) {
  MyStruct *t_a = (MyStruct *)a;
  MyStruct *t_b = (MyStruct *)b;
  return prv_cmp(t_a->number, t_b->number);
}

void test_sort__sort_structs(void) {
  MyStruct array[] = {
    {.number = 6 },
    {.number = -1 },
    {.number = 8 },
    {.number = -123 },
  };

  sort_bubble(array, ARRAY_LENGTH(array), sizeof(MyStruct), prv_MyStruct_cmp);

  MyStruct sorted[] = {
    {.number = -123 },
    {.number = -1 },
    {.number = 6 },
    {.number = 8 },
  };
  cl_assert_equal_m(array, sorted, sizeof(array));
}
