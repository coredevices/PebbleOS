/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/status_codes.h"

///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void prefs_db_init(void);

status_t prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int prefs_db_get_len(const uint8_t *key, int key_len);

status_t prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

status_t prefs_db_delete(const uint8_t *key, int key_len);

status_t prefs_db_flush(void);
