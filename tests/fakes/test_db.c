/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_db.h"

#include <string.h>

#include "ram_storage.h"

#include "kernel/pbl_malloc.h"

struct {
  RamStorage ram_storage;
} s_test_db;

void test_db_init(void) {
  s_test_db.ram_storage = ram_storage_create();
}

status_t test_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return ram_storage_insert(&s_test_db.ram_storage, key, key_len, val, val_len);
}

int test_db_get_len(const uint8_t *key, int key_len) {
  return ram_storage_get_len(&s_test_db.ram_storage, key, key_len);
}

status_t test_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return ram_storage_read(&s_test_db.ram_storage, key, key_len, val_out, val_len);
}

status_t test_db_delete(const uint8_t *key, int key_len) {
  return ram_storage_delete(&s_test_db.ram_storage, key, key_len);
}

status_t test_db_flush(void) {
  return ram_storage_flush(&s_test_db.ram_storage);
}

status_t test_db_is_dirty(bool *is_dirty_out) {
  return ram_storage_is_dirty(&s_test_db.ram_storage, is_dirty_out);
}

bool prv_dirty_items_each_cb(RamStorageEntry *entry, void *context) {
  BlobDBDirtyItem **dirty_items = context;
  if (entry->flags & RamStorageFlagDirty) {
    BlobDBDirtyItem *new_item = kernel_zalloc_check(sizeof(BlobDBDirtyItem) + entry->key_len);
    memcpy(new_item->key, entry->key, entry->key_len);
    new_item->key_len = entry->key_len;
    *dirty_items = (BlobDBDirtyItem *)list_insert_before((ListNode *)*dirty_items,
                                                         (ListNode *)new_item);
  }

  return true;
}

BlobDBDirtyItem *test_db_get_dirty_list(void) {
  BlobDBDirtyItem *dirty_items = NULL;
  ram_storage_each(&s_test_db.ram_storage, prv_dirty_items_each_cb, &dirty_items);
  return dirty_items;
}

status_t test_db_mark_synced(uint8_t *key, int key_len) {
  return ram_storage_mark_synced(&s_test_db.ram_storage, key, key_len);
}
