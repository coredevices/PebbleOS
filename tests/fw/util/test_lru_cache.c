/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/lru_cache.h"

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_passert.h"

#include <string.h>

const uint8_t CACHE_BUFFER_SIZE = 80;
uint8_t s_buffer[CACHE_BUFFER_SIZE];
LRUCache s_cache;

void test_lru_cache__initialize(void) {
  lru_cache_init(&s_cache, sizeof(uint32_t), s_buffer, CACHE_BUFFER_SIZE);
}

void test_lru_cache__cleanup(void) {
  lru_cache_flush(&s_cache);
}

void test_lru_cache__one_put(void) {
  uint32_t input = 0xdeadbeef;
  lru_cache_put(&s_cache, 1, &input);
  uint32_t *output = lru_cache_get(&s_cache, 1);
  cl_assert(output);
  cl_assert(*output == input);
}

void test_lru_cache__one_put_two_get(void) {
  uint32_t input = 0xdeadbeef;
  lru_cache_put(&s_cache, 1, &input);

  uint32_t *output;
  for (int i = 0; i < 2; i++) {
    output = lru_cache_get(&s_cache, 1);
    cl_assert(output);
    cl_assert(*output == input);
  }
}

void test_lru_cache__two_puts_one_get(void) {
  uint32_t input = 0xdeadbeef;
  lru_cache_put(&s_cache, 1, &input);
  lru_cache_put(&s_cache, 1, &input);

  uint32_t *output;
  output = lru_cache_get(&s_cache, 1);
  cl_assert(output);
  cl_assert(*output == input);
}

void test_lru_cache__flush(void) {
  uint32_t input = 0xdeadbeef;
  lru_cache_put(&s_cache, 1, &input);
  lru_cache_flush(&s_cache);
  uint32_t *output = lru_cache_get(&s_cache, 1);
  cl_assert(output == NULL);
}

void test_lru_cache__evict(void) {
  for (int i = 0; i <= CACHE_BUFFER_SIZE / (sizeof(CacheEntry) + sizeof(uint32_t)); ++i) {
    uint32_t input = i;
    lru_cache_put(&s_cache, i, &input);
  }
  uint32_t *output = lru_cache_get(&s_cache, 0);
  // check that the oldest entry got evicted
  cl_assert(output == NULL);
  for (int i = 1; i <= CACHE_BUFFER_SIZE / (sizeof(CacheEntry) + sizeof(uint32_t)); ++i) {
    output = lru_cache_get(&s_cache, i);
    // check that the others are still around
    cl_assert(output);
    cl_assert(*output == i);
  }
}

void test_lru_cache__use_and_evict(void) {
  int i;
  for (i = 0; i < CACHE_BUFFER_SIZE / (sizeof(CacheEntry) + sizeof(uint32_t)); ++i) {
    uint32_t input = i;
    lru_cache_put(&s_cache, i, &input);
  }
  // use entry 0 to keep it around
  uint32_t *output = lru_cache_get(&s_cache, 0);
  cl_assert(output);
  cl_assert(*output == 0);

  // add one more entry
  uint32_t input = i;
  lru_cache_put(&s_cache, i, &input);

  // check that entry 0 is around
  output = lru_cache_get(&s_cache, 0);
  cl_assert(output);
  cl_assert(*output == 0);

  // check that entry 1 got evicted
  output = lru_cache_get(&s_cache, 1);
  cl_assert(output == NULL);

  // check that the others are still around
  for (int i = 2; i <= CACHE_BUFFER_SIZE / (sizeof(CacheEntry) + sizeof(uint32_t)); ++i) {
    output = lru_cache_get(&s_cache, i);
    // check that the others are still around
    cl_assert(output);
    cl_assert(*output == i);
  }
}

