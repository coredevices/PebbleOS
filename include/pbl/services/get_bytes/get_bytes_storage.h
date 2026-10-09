/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>

#include <pbl/services/get_bytes/get_bytes.h>

/**
 * @defgroup services_get_bytes_get_bytes_storage Get bytes storage
 * @ingroup services_get_bytes
 * @brief Backends reading the transferred objects.
 *
 * Each @ref GetBytesObjectType maps to a backend implementing setup, size, read and cleanup.
 * File and flash backends are not built in release or recovery firmware.
 * @{
 */

/** @brief Storage backend type. */
typedef enum {
  /** Unknown. */
  GetBytesStorageTypeUnknown,
  /** Core dump flash region. */
  GetBytesStorageTypeCoredump,
  /** PFS file. */
  GetBytesStorageTypeFile,
  /** Raw flash. */
  GetBytesStorageTypeFlash
} GetBytesStorageType;

struct GetBytesStorageImplementation;
/** @brief Backend operations, private to the get bytes service. */
typedef struct GetBytesStorageImplementation GetBytesStorageImplementation;

/** @brief Storage of an object being transferred. */
typedef struct {
  /** Backend operations. */
  const GetBytesStorageImplementation *impl;

  /** Backend private data. */
  void *impl_data;

  /** Offset of the next byte to read, advanced by gb_storage_read_next_chunk(). */
  uint32_t current_offset;
} GetBytesStorage;

/** @brief Request parameters passed to the backend setup. */
typedef struct {
  /** File name, for @ref GetBytesStorageTypeFile. */
  char *filename;
  /** Start address, for @ref GetBytesStorageTypeFlash. */
  uint32_t flash_start_addr;
  /** Length in bytes, for @ref GetBytesStorageTypeFlash. */
  uint32_t flash_len;
  /** Only return a core dump not read yet, for @ref GetBytesStorageTypeCoredump. */
  bool only_get_new_coredump;
} GetBytesStorageInfo;

/**
 * @brief Set up the storage of an object, e.g. allocate memory or open a file.
 *
 * @param[out] storage Storage.
 * @param object_type Object type, selecting the backend.
 * @param info Request parameters.
 * @return true on success.
 */
bool gb_storage_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                      GetBytesStorageInfo *info);

/**
 * @brief Get the size of the object.
 *
 * @param storage Storage.
 * @param[out] size Object size in bytes.
 * @return @ref GET_BYTES_OK, or the error to report to the phone.
 */
GetBytesInfoErrorCode gb_storage_get_size(GetBytesStorage *storage, uint32_t *size);

/**
 * @brief Read the next chunk of the object.
 *
 * @param storage Storage.
 * @param[out] buffer Destination.
 * @param len Number of bytes to read.
 * @return true on success.
 */
bool gb_storage_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len);

/**
 * @brief Release the storage.
 *
 * @param storage Storage.
 * @param successful The whole object was sent. A sent core dump is marked as read.
 */
void gb_storage_cleanup(GetBytesStorage *storage, bool successful);

/** @} */
