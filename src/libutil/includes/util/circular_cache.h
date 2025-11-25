/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/order.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! @note Needs to handle NULL items gracefully
typedef void (*CircularCacheItemDestructor)(void *item);

//! Array-backed circular cache
typedef struct {
  uint8_t* cache; //<! Pointer to the array
  size_t item_size; //<! Size of the array element in bytes
  int next_erased_item_idx; //<! Next array element to be deleted
  int total_items;
  Comparator compare_cb;
  CircularCacheItemDestructor item_destructor;
} CircularCache;

void circular_cache_init(CircularCache* c, uint8_t* buffer, size_t item_size,
    int total_items, Comparator compare_cb);

//! Add a destructor to be called when an item is evicted from the circular cache.
//! @note the destructor needs to handle NULL items gracefully
void circular_cache_set_item_destructor(CircularCache *c, CircularCacheItemDestructor destructor);

//! @return True if the cache contains the data
//! @note Item must be of size item_size
bool circular_cache_contains(CircularCache* c, void* item);

//! @return Pointer to buffer of entry in cache that contains the data
//! @note Item must be of size item_size
void *circular_cache_get(CircularCache* c, void* theirs);

//! Push data of size item_size into the circular cache
//! Overwrites the item at next_erased_item_idx
void circular_cache_push(CircularCache* c, void* item);

//! Fills a circular cache with the representation an item, useful for non-zero clearing a cache
//! @note this will assert if an item destructor is set
void circular_cache_fill(CircularCache *c, uint8_t *item);

//! Flushes the buffer, calling destructors for each item in the cache. The calling module must
//! be able to differentiate between a valid and invalid entry in the cache (e.g. the cache is not
//! yet filled, so it has entries with zeroed out data).
void circular_cache_flush(CircularCache *c);
