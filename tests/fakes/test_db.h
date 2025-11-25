/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "services/normal/blob_db/api.h"

#include "system/status_codes.h"

void test_db_init(void);

status_t test_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int test_db_get_len(const uint8_t *key, int key_len);

status_t test_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

status_t test_db_delete(const uint8_t *key, int key_len);

status_t test_db_flush(void);

status_t test_db_is_dirty(bool *is_dirty_out);

BlobDBDirtyItem *test_db_get_dirty_list(void);

status_t test_db_mark_synced(uint8_t *key, int key_len);
