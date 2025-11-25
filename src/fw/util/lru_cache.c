/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lru_cache.h"

#include "system/passert.h"

#include <string.h>

void lru_cache_init(LRUCache* c, size_t item_size, uint8_t *buffer, size_t buffer_size) {
  *c = (LRUCache) {
    .buffer = buffer,
    .item_size = item_size,
    .max_items = buffer_size / (item_size + sizeof(CacheEntry)),
    .least_recent = NULL
  };
}

static CacheEntry *entry_for_index(LRUCache *c, int index) {
  return ((CacheEntry *)(c->buffer + index * (sizeof(CacheEntry) + c->item_size)));
}

void lru_cache_flush(LRUCache *c) {
  c->least_recent = NULL;
}

void *lru_cache_get(LRUCache* c, uint32_t key) {
  // cur_ptr is a pointer-to-pointer to the more_recent
  // field in the parent of the current entry
  CacheEntry **cur_ptr = &c->least_recent;
  CacheEntry *found = NULL;

  for (int i = 0; i < c->max_items; ++i) {
    CacheEntry *cur_entry = *cur_ptr;
    if (cur_entry == NULL) {
      break;
    }
    if (cur_entry->key == key) {
      *cur_ptr = cur_entry->more_recent;
      found = cur_entry;
    }
    if (*cur_ptr == NULL) {
      break;
    }
    cur_ptr = &cur_entry->more_recent;
  }

  // cur_ptr should point to the last pointer in the list
  PBL_ASSERTN(*cur_ptr == NULL);

  if (found) {
    found->more_recent = NULL;
    *cur_ptr = found;
    return (found->data);
  } else {
    return (NULL);
  }
}

void lru_cache_put(LRUCache* c, uint32_t key, void* item) {
  // cur_ptr is a pointer-to-pointer to the more_recent
  // field in the parent of the current entry
  CacheEntry **cur_ptr = &c->least_recent;
  CacheEntry *new = NULL;

  for (int i = 0; i < c->max_items; ++i) {
    CacheEntry *cur_entry = *cur_ptr;
    if (cur_entry == NULL) {
      // cache is not full
      new = entry_for_index(c, i);
      break;
    } else if (cur_entry->key == key) {
      // key already in cache, update
      *cur_ptr = cur_entry->more_recent;
      new = cur_entry;
    }
    if (*cur_ptr == NULL) {
      break;
    }
    cur_ptr = &cur_entry->more_recent;
  }

  // cur_ptr should point to the last pointer in the list
  PBL_ASSERTN(*cur_ptr == NULL);

  if (new == NULL) {
    // cache full, evict LRU
    new = c->least_recent;
    c->least_recent = new->more_recent;
  }

  new->more_recent = NULL;
  new->key = key;
  memcpy(new->data, item, c->item_size);
  *cur_ptr = new;
}

