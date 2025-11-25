/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/keyed_circular_cache.h"

#include "util/size.h"

#include "clar.h"

#include <string.h>

#define TEST_BUFFER_SIZE (3)

typedef struct {
  uint32_t data[4];
} TestCacheItem;

typedef struct {
  uint32_t key;
  TestCacheItem item;
} TestCacheDefinition;

static KeyedCircularCache s_test_cache;
static KeyedCircularCacheKey s_cache_keys[TEST_BUFFER_SIZE];
static TestCacheItem s_cache_buffer[TEST_BUFFER_SIZE];
static TestCacheItem ZERO_ITEM = {};
static const TestCacheDefinition s_test_data[] = {
  { .key = 0x12345678,
    .item = { .data = { 0xDEADCAFE, 0xBEEFBABE, 0xF00DD00D, 0xDEFACED1, }, },
  },
  { .key = 0x9ABCDEF0,
    .item = { .data = { 0x13579BDF, 0x02468ACE, 0xFEDCBA98, 0x76543210, }, },
  },
  { .key = 0x01238ACE,
    .item = { .data = { 0x012389AB, 0x4567CDEF, 0x014589CD, 0x2367ABEF, }, },
  },
  { .key = 0x45679BDF,
    .item = { .data = { 0xFEDC7654, 0xBA983210, 0xFEBA7632, 0xDC985410, }, },
  },
};

// setup and teardown
void test_keyed_circular_cache__initialize(void) {
  memset(s_cache_buffer, 0, sizeof(s_cache_buffer));
  memset(s_cache_keys, 0, sizeof(s_cache_keys));
  keyed_circular_cache_init(&s_test_cache, s_cache_keys, s_cache_buffer, sizeof(TestCacheItem),
                            TEST_BUFFER_SIZE);
}

void test_keyed_circular_cache__cleanup(void) {
}

// tests
static void prv_push(int index) {
  keyed_circular_cache_push(&s_test_cache, s_test_data[index].key, &s_test_data[index].item);
}

static TestCacheItem *prv_get(KeyedCircularCacheKey key) {
  return keyed_circular_cache_get(&s_test_cache, key);
}

static void prv_test_backing_data(int cache_idx, int data_idx) {
  cl_assert_equal_i(s_cache_keys[cache_idx], s_test_data[data_idx].key);
  cl_assert_equal_m(&s_cache_buffer[cache_idx], &s_test_data[data_idx].item,
                    sizeof(TestCacheItem));
}

static void prv_test_backing_data_empty(int cache_idx) {
  cl_assert_equal_i(s_cache_keys[cache_idx], 0);
  cl_assert_equal_m(&s_cache_buffer[cache_idx], &ZERO_ITEM, sizeof(TestCacheItem));
}

static void prv_test_get_miss(int data_idx) {
  cl_assert(!prv_get(s_test_data[data_idx].key));
}

static void prv_test_get_hit(int data_idx, int cache_idx) {
  TestCacheItem *data = prv_get(s_test_data[data_idx].key);
  cl_assert(data);
  cl_assert_equal_m(data, &s_test_data[data_idx].item, sizeof(TestCacheItem));
  cl_assert(data == &s_cache_buffer[cache_idx]);
}

void test_keyed_circular_cache__push(void) {
  prv_push(0);
  prv_test_backing_data(0, 0);
  prv_test_backing_data_empty(1);
  prv_test_backing_data_empty(2);

  prv_push(1);
  prv_test_backing_data(0, 0);
  prv_test_backing_data(1, 1);
  prv_test_backing_data_empty(2);

  prv_push(2);
  prv_test_backing_data(0, 0);
  prv_test_backing_data(1, 1);
  prv_test_backing_data(2, 2);

  prv_push(3);
  prv_test_backing_data(0, 3);
  prv_test_backing_data(1, 1);
  prv_test_backing_data(2, 2);

  prv_push(0);
  prv_test_backing_data(0, 3);
  prv_test_backing_data(1, 0);
  prv_test_backing_data(2, 2);

  prv_push(1);
  prv_test_backing_data(0, 3);
  prv_test_backing_data(1, 0);
  prv_test_backing_data(2, 1);
}

void test_circular_cache__get(void) {
  prv_test_get_miss(0);
  prv_test_get_miss(1);
  prv_test_get_miss(2);
  prv_test_get_miss(3);

  prv_push(0);
  prv_test_get_hit(0, 0);
  prv_test_get_miss(1);
  prv_test_get_miss(2);
  prv_test_get_miss(3);

  prv_push(1);
  prv_test_get_hit(0, 0);
  prv_test_get_hit(1, 1);
  prv_test_get_miss(2);
  prv_test_get_miss(3);

  prv_push(2);
  prv_test_get_hit(0, 0);
  prv_test_get_hit(1, 1);
  prv_test_get_hit(2, 2);
  prv_test_get_miss(3);

  prv_push(3);
  prv_test_get_miss(0);
  prv_test_get_hit(1, 1);
  prv_test_get_hit(2, 2);
  prv_test_get_hit(3, 0);

  prv_push(2);
  prv_test_get_miss(0);
  prv_test_get_miss(1);
  prv_test_get_hit(2, 1);
  prv_test_get_hit(3, 0);

  prv_push(0);
  prv_test_get_hit(0, 2);
  prv_test_get_miss(1);
  prv_test_get_hit(2, 1);
  prv_test_get_hit(3, 0);

  prv_push(1);
  prv_test_get_hit(0, 2);
  prv_test_get_hit(1, 0);
  prv_test_get_hit(2, 1);
  prv_test_get_miss(3);

  prv_push(3);
  prv_test_get_hit(0, 2);
  prv_test_get_hit(1, 0);
  prv_test_get_miss(2);
  prv_test_get_hit(3, 1);
}
