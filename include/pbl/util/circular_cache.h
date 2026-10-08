/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/order.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_circular_cache Circular cache
 * @ingroup util
 * @brief Fixed-size, array-backed cache that evicts its oldest item.
 *
 * Items are found by linear search with a comparator. See also @ref util_keyed_circular_cache.
 * @{
 */

/**
 * @brief Destructor called on an item about to be evicted or flushed.
 *
 * It is also called on slots that never held an item, so it must recognize those (e.g. all
 * zeros).
 *
 * @param item Item.
 */
typedef void (*CircularCacheItemDestructor)(void *item);

/** @brief Circular cache state. */
typedef struct {
  /** Item array. */
  uint8_t *cache;
  /** Size of an item in bytes. */
  size_t item_size;
  /** Index of the next item to evict. */
  int next_erased_item_idx;
  /** Number of items in @ref cache. */
  int total_items;
  /** Comparator, returns 0 for matching items. */
  Comparator compare_cb;
  /** Optional item destructor. */
  CircularCacheItemDestructor item_destructor;
} CircularCache;

/**
 * @brief Initialize a circular cache.
 *
 * @param[out] c Circular cache.
 * @param buffer Item array of @p total_items items of @p item_size bytes, initialized by the
 * caller.
 * @param item_size Size of an item in bytes.
 * @param total_items Number of items.
 * @param compare_cb Comparator, returns 0 for matching items.
 */
void circular_cache_init(CircularCache *c, uint8_t *buffer, size_t item_size, int total_items,
                         Comparator compare_cb);

/**
 * @brief Set the destructor called on items when they are evicted or flushed.
 *
 * @param c Circular cache.
 * @param destructor Destructor, NULL for none.
 */
void circular_cache_set_item_destructor(CircularCache *c, CircularCacheItemDestructor destructor);

/**
 * @brief Check whether the cache contains an item.
 *
 * @param c Circular cache.
 * @param item Item to compare against, of the cache's item size.
 * @return true if an item matches.
 */
bool circular_cache_contains(CircularCache *c, void *item);

/**
 * @brief Find an item in the cache.
 *
 * @param c Circular cache.
 * @param theirs Item to compare against, of the cache's item size.
 * @return Matching item in the cache, or NULL.
 */
void *circular_cache_get(CircularCache *c, void *theirs);

/**
 * @brief Copy an item into the cache, evicting the oldest one.
 *
 * @param c Circular cache.
 * @param item Item, of the cache's item size.
 */
void circular_cache_push(CircularCache *c, void *item);

/**
 * @brief Set every slot of the cache to a copy of an item.
 *
 * Useful to clear a cache to a non-zero value. Asserts if an item destructor is set.
 *
 * @param c Circular cache.
 * @param item Item, of the cache's item size.
 */
void circular_cache_fill(CircularCache *c, uint8_t *item);

/**
 * @brief Call the destructor on every slot and restart eviction from the first one.
 *
 * The item data is left in place.
 *
 * @param c Circular cache.
 */
void circular_cache_flush(CircularCache *c);

/** @} */
