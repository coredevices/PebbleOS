/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/order.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t KeyedCircularCacheKey;

//! Array-backed circular cache, optimized for data cache efficiency
typedef struct {
  KeyedCircularCacheKey *cache_keys;
  uint8_t *cache_data; //<! Pointer to the array
  size_t item_size; //<! Size of the array element in bytes
  size_t next_item_to_erase_idx; //<! Next array element to be deleted
  size_t total_items;
} KeyedCircularCache;

void keyed_circular_cache_init(KeyedCircularCache *c, KeyedCircularCacheKey *key_buffer,
                               void *data_buffer, size_t item_size, size_t total_items);

//! @return Pointer to buffer of entry in cache that contains the data
//! @note Item must be of size item_size
void *keyed_circular_cache_get(KeyedCircularCache *c, KeyedCircularCacheKey key);

//! Push data of size item_size into the circular cache
//! Overwrites the item at next_item_to_erase_idx
void keyed_circular_cache_push(KeyedCircularCache *c, KeyedCircularCacheKey key,
                               const void *item);
