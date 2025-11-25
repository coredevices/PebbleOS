/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ram_storage.h"

#include <string.h>

#include "kernel/pbl_malloc.h"

typedef struct {
  const uint8_t *key;
  int key_len;
} KeyInfo;

static bool prv_filter_callback(ListNode *found_node, void *data) {
  RamStorageEntry *entry = (RamStorageEntry *)found_node;
  KeyInfo *info = (KeyInfo *)data;
  if (entry->key_len == info->key_len) {
    return (memcmp(entry->key, info->key, entry->key_len) == 0);
  }
  return false;
}

static RamStorageEntry *prv_get_entry(RamStorageEntry *entries, const uint8_t *key, int key_len) {
  KeyInfo info = {
    .key = key,
    .key_len = key_len,
  };
  return (RamStorageEntry *)list_find((ListNode *)entries, prv_filter_callback, &info);
}

static void prv_delete(RamStorageEntry **entries, RamStorageEntry *entry) {
  list_remove((ListNode *)entry, (ListNode **)entries, NULL);
  kernel_free(entry->key);
  kernel_free(entry->val);
  kernel_free(entry);
}

status_t ram_storage_insert(RamStorage *storage,
    const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  // delete entry if it already exists
  RamStorageEntry *entry = prv_get_entry(storage->entries, key, key_len);
  if (entry) {
    prv_delete(&storage->entries, entry);
  }

  // Allocate the entry struct
  entry = kernel_malloc_check(sizeof(RamStorageEntry));
  *entry = (RamStorageEntry){};
  // Allocate key & values and copy them over
  entry->key = kernel_malloc_check(key_len);
  memcpy(entry->key, key,  key_len);
  entry->val = kernel_malloc_check(val_len);
  memcpy(entry->val, val, val_len);
  entry->key_len = key_len;
  entry->val_len = val_len;
  entry->flags |= RamStorageFlagDirty;
  // Add to list
  storage->entries = (RamStorageEntry *)list_prepend((ListNode *)storage->entries, &entry->node);

  return 0;
}

int ram_storage_get_len(RamStorage *storage, const uint8_t *key, int key_len) {
  RamStorageEntry *entry = prv_get_entry(storage->entries, key, key_len);
  if (entry) {
    return entry->val_len;
  }

  return E_DOES_NOT_EXIST;
}

status_t ram_storage_read(RamStorage *storage,
    const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  RamStorageEntry *entry = prv_get_entry(storage->entries, key, key_len);
  if (entry) {
    memcpy(val_out, entry->val, val_len);
    return S_SUCCESS;
  }

  return E_DOES_NOT_EXIST;
}

status_t ram_storage_delete(RamStorage *storage, const uint8_t *key, int key_len) {
  RamStorageEntry *entry = prv_get_entry(storage->entries, key, key_len);
  if (entry) {
    prv_delete(&storage->entries, entry);
    return S_SUCCESS;
  }

  return E_DOES_NOT_EXIST;
}

status_t ram_storage_flush(RamStorage *storage) {
  while (storage->entries) {
    prv_delete(&storage->entries, storage->entries);
  }

  return S_SUCCESS;
}

RamStorage ram_storage_create(void) {
  RamStorage storage;
  storage.entries = NULL;
  return storage;
}

static bool prv_find_dirty(ListNode *found_node, void *data) {
  RamStorageEntry *entry = (RamStorageEntry *)found_node;
  return !!(entry->flags & RamStorageFlagDirty);
}

status_t ram_storage_is_dirty(RamStorage *storage, bool *is_dirty_out) {
  *is_dirty_out = (list_find((ListNode *)storage->entries, prv_find_dirty, NULL) != NULL);
  return S_SUCCESS;
}

status_t ram_storage_mark_synced(RamStorage *storage, uint8_t *key, int key_len) {
  RamStorageEntry *entry = prv_get_entry(storage->entries, key, key_len);
  if (entry) {
    entry->flags &= ~RamStorageFlagDirty;
    return S_SUCCESS;
  }

  return E_DOES_NOT_EXIST;
}

status_t ram_storage_each(RamStorage *storage, RamStorageEachCb cb, void *context) {
  RamStorageEntry *cur = storage->entries;
  while (cur) {
    if (!cb(cur, context)) {
      break;
    }
    cur = (RamStorageEntry *)cur->node.next;
  }

  return S_SUCCESS;
}
