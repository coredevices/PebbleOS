/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "put_bytes_storage.h"

#include <stdbool.h>

typedef struct PutBytesStorageImplementation {
  bool (*init)(PutBytesStorage *storage, PutBytesObjectType object_type,
               uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset);

  uint32_t (*get_max_size)(PutBytesObjectType object_type);

  void (*write)(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer, uint32_t length);

  uint32_t (*calculate_crc)(PutBytesStorage *storage, PutBytesCrcType crc_type);

  void (*deinit)(PutBytesStorage *storage, bool is_success);
} PutBytesStorageImplementation;
