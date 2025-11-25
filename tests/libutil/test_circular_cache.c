/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/circular_cache.h"

#include "util/size.h"

#include "clar.h"

#include <string.h>

#define NUM_TEST_ITEMS (3)

typedef struct {
  int id;
  bool *freed;
} TestCacheItem;
// Stubs


static bool s_free_flags[NUM_TEST_ITEMS];
static CircularCache s_test_cache;
static TestCacheItem s_cache_buffer[NUM_TEST_ITEMS];
static TestCacheItem s_test_item[NUM_TEST_ITEMS] = {
    { .id = 1, .freed = &s_free_flags[0] },
    { .id = 2, .freed = &s_free_flags[1] },
    { .id = 3, .freed = &s_free_flags[2] },
  };
static const TestCacheItem ZERO_ITEM = {};

static void prv_destructor(void *item) {
  if (((TestCacheItem *)item)->freed) {
    *((TestCacheItem *)item)->freed = true;
  }
}

static int prv_comparator(void *a, void *b) {
  return ((TestCacheItem *)a)->id - ((TestCacheItem *)b)->id;
}

// setup and teardown
void test_circular_cache__initialize(void) {
  memset(s_cache_buffer, 0, sizeof(s_cache_buffer));
  circular_cache_init(&s_test_cache, (uint8_t *)s_cache_buffer, sizeof(TestCacheItem),
                      NUM_TEST_ITEMS, prv_comparator);
  for (int i = 0; i < ARRAY_LENGTH(s_test_item); i++) {
    s_free_flags[i] = false;
  }
}

void test_circular_cache__cleanup(void) {
}

// tests
void test_circular_cache__push(void) {
  circular_cache_set_item_destructor(&s_test_cache, prv_destructor);

  circular_cache_push(&s_test_cache, &s_test_item[0]);
  cl_assert_equal_m(&s_cache_buffer[0], &s_test_item[0], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[1], &ZERO_ITEM, sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[2], &ZERO_ITEM, sizeof(TestCacheItem));

  circular_cache_push(&s_test_cache, &s_test_item[2]);
  cl_assert_equal_m(&s_cache_buffer[0], &s_test_item[0], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[1], &s_test_item[2], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[2], &ZERO_ITEM, sizeof(TestCacheItem));

  circular_cache_push(&s_test_cache, &s_test_item[1]);
  cl_assert_equal_m(&s_cache_buffer[0], &s_test_item[0], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[1], &s_test_item[2], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[2], &s_test_item[1], sizeof(TestCacheItem));

  circular_cache_push(&s_test_cache, &s_test_item[1]);
  cl_assert_equal_m(&s_cache_buffer[0], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[1], &s_test_item[2], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[2], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert(*s_test_item[0].freed);

  circular_cache_push(&s_test_cache, &s_test_item[1]);
  cl_assert_equal_m(&s_cache_buffer[0], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[1], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[2], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert(*s_test_item[2].freed);
}

void test_circular_cache__get(void) {
  cl_assert(!circular_cache_get(&s_test_cache, &s_test_item[0]));
  cl_assert(!circular_cache_get(&s_test_cache, &s_test_item[1]));
  cl_assert(!circular_cache_get(&s_test_cache, &s_test_item[2]));

  circular_cache_push(&s_test_cache, &s_test_item[0]);
  circular_cache_push(&s_test_cache, &s_test_item[1]);

  cl_assert(!circular_cache_get(&s_test_cache, &s_test_item[2]));
  cl_assert_equal_p(circular_cache_get(&s_test_cache, &s_test_item[0]), &s_cache_buffer[0]);
  cl_assert_equal_p(circular_cache_get(&s_test_cache, &s_test_item[1]), &s_cache_buffer[1]);

  circular_cache_push(&s_test_cache, &s_test_item[1]);
  cl_assert(!circular_cache_get(&s_test_cache, &s_test_item[2]));
  cl_assert_equal_p(circular_cache_get(&s_test_cache, &s_test_item[1]), &s_cache_buffer[1]);
}

void test_circular_cache__contains(void) {
  cl_assert(!circular_cache_contains(&s_test_cache, &s_test_item[0]));
  cl_assert(!circular_cache_contains(&s_test_cache, &s_test_item[1]));
  cl_assert(!circular_cache_contains(&s_test_cache, &s_test_item[2]));

  circular_cache_push(&s_test_cache, &s_test_item[0]);
  circular_cache_push(&s_test_cache, &s_test_item[1]);

  cl_assert(!circular_cache_contains(&s_test_cache, &s_test_item[2]));
  cl_assert(circular_cache_contains(&s_test_cache, &s_test_item[0]));
  cl_assert(circular_cache_contains(&s_test_cache, &s_test_item[1]));
}

void test_circular_cache__fill(void) {
  circular_cache_fill(&s_test_cache, (uint8_t *)&s_test_item[1]);
  cl_assert_equal_m(&s_cache_buffer[0], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[1], &s_test_item[1], sizeof(TestCacheItem));
  cl_assert_equal_m(&s_cache_buffer[2], &s_test_item[1], sizeof(TestCacheItem));
}

void test_circular_cache__flush(void) {
  circular_cache_set_item_destructor(&s_test_cache, prv_destructor);

  circular_cache_push(&s_test_cache, &s_test_item[0]);

  circular_cache_flush(&s_test_cache);
  cl_assert(*s_test_item[0].freed);
  cl_assert(!(*s_test_item[1].freed));
  cl_assert(!(*s_test_item[2].freed));

  memset(s_cache_buffer, 0, sizeof(s_cache_buffer));
  *s_test_item[0].freed = false;

  circular_cache_push(&s_test_cache, &s_test_item[0]);
  circular_cache_push(&s_test_cache, &s_test_item[1]);
  circular_cache_push(&s_test_cache, &s_test_item[2]);
  circular_cache_flush(&s_test_cache);
  cl_assert(*s_test_item[0].freed);
  cl_assert(*s_test_item[1].freed);
  cl_assert(*s_test_item[2].freed);
}
