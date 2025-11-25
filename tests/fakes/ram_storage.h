/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "system/status_codes.h"
#include "util/list.h"

typedef enum {
  RamStorageFlagDirty = 1 << 0,
} RamStorageFlag;

typedef struct {
  ListNode node;
  uint8_t flags;
  uint8_t *key;
  int key_len;
  uint8_t *val;
  int val_len;
} RamStorageEntry;

typedef struct {
  RamStorageEntry *entries;
} RamStorage;

RamStorage ram_storage_create(void);

status_t ram_storage_insert(RamStorage *storage,
    const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int ram_storage_get_len(RamStorage *storage,
    const uint8_t *key, int key_len);

status_t ram_storage_read(RamStorage *storage,
    const uint8_t *key, int key_len, uint8_t *val_out, int val_len);

status_t ram_storage_delete(RamStorage *storage,
    const uint8_t *key, int key_len);

status_t ram_storage_flush(RamStorage *storage);

typedef bool (RamStorageEachCb)(RamStorageEntry *entry, void *context);

status_t ram_storage_each(RamStorage *storage, RamStorageEachCb cb, void *context);

status_t ram_storage_is_dirty(RamStorage *storage, bool *is_dirty_out);

status_t ram_storage_mark_synced(RamStorage *storage, uint8_t *key, int key_len);
