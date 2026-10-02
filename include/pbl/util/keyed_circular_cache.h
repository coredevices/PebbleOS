/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/order.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_keyed_circular_cache Keyed circular cache
 * @ingroup util
 * @brief Fixed-size cache of items looked up by integer key, evicting the oldest item.
 *
 * Keys are stored apart from the items so lookups scan a compact array. Most recently pushed
 * items are found first.
 *
 * @code{.c}
 * static KeyedCircularCacheKey s_keys[8];
 * static struct glyph s_glyphs[8];
 * static KeyedCircularCache s_cache;
 *
 * keyed_circular_cache_init(&s_cache, s_keys, s_glyphs, sizeof(struct glyph), 8);
 * struct glyph *g = keyed_circular_cache_get(&s_cache, codepoint);
 * if (!g) {
 *   load_glyph(codepoint, &tmp);
 *   keyed_circular_cache_push(&s_cache, codepoint, &tmp);
 * }
 * @endcode
 * @{
 */

/** @brief Key of a keyed circular cache. */
typedef uint32_t KeyedCircularCacheKey;

/** @brief Keyed circular cache state. */
typedef struct {
  /** Key array. */
  KeyedCircularCacheKey *cache_keys;
  /** Item array. */
  uint8_t *cache_data;
  /** Size of an item in bytes. */
  size_t item_size;
  /** Index of the next item to evict. */
  size_t next_item_to_erase_idx;
  /** Number of items. */
  size_t total_items;
} KeyedCircularCache;

/**
 * @brief Initialize a keyed circular cache.
 *
 * The key array is used as is, so initialize it with keys that are never looked up.
 *
 * @param[out] c Cache.
 * @param key_buffer Key array of @p total_items keys.
 * @param data_buffer Item array of @p total_items items.
 * @param item_size Size of an item in bytes.
 * @param total_items Number of items.
 */
void keyed_circular_cache_init(KeyedCircularCache *c, KeyedCircularCacheKey *key_buffer,
                               void *data_buffer, size_t item_size, size_t total_items);

/**
 * @brief Find an item by key.
 *
 * @param c Cache.
 * @param key Key.
 * @return Item in the cache, or NULL.
 */
void *keyed_circular_cache_get(KeyedCircularCache *c, KeyedCircularCacheKey key);

/**
 * @brief Copy an item into the cache, evicting the oldest one.
 *
 * @param c Cache.
 * @param key Key of the item.
 * @param item Item, of the cache's item size.
 */
void keyed_circular_cache_push(KeyedCircularCache *c, KeyedCircularCacheKey key, const void *item);

/** @} */
