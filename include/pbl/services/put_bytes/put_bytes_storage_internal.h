/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "put_bytes_storage.h"

#include <stdbool.h>

/**
 * @defgroup services_put_bytes_put_bytes_storage_internal Put bytes storage backend interface
 * @ingroup services_put_bytes_put_bytes_storage
 * @brief Operations implemented by each storage backend.
 * @{
 */

/** @brief Storage backend operations, mirroring the pb_storage_*() functions. */
typedef struct PutBytesStorageImplementation {
  /** Initialize storage, see pb_storage_init(). */
  bool (*init)(PutBytesStorage *storage, PutBytesObjectType object_type, uint32_t total_size,
               PutBytesStorageInfo *info, uint32_t append_offset);

  /** Get the largest object size supported for a type. */
  uint32_t (*get_max_size)(PutBytesObjectType object_type);

  /** Write data, see pb_storage_write(). */
  void (*write)(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer, uint32_t length);

  /** Compute the CRC, see pb_storage_calculate_crc(). */
  uint32_t (*calculate_crc)(PutBytesStorage *storage, PutBytesCrcType crc_type);

  /** Release storage, see pb_storage_deinit(). */
  void (*deinit)(PutBytesStorage *storage, bool is_success);
} PutBytesStorageImplementation;

/** @} */
