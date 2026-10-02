/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @addtogroup services_blob_db
 * @{
 */

/** @brief Kind of change reported by a @c PEBBLE_BLOBDB_EVENT. */
typedef enum BlobDBEventType {
  /** A record was inserted or replaced. */
  BlobDBEventTypeInsert,
  /** A record was deleted. */
  BlobDBEventTypeDelete,
  /** All records of the database were deleted. */
  BlobDBEventTypeFlush,
} BlobDBEventType;

/** @} */
