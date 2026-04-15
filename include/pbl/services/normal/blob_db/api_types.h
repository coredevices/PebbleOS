/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum BlobDBEventType {
  BlobDBEventTypeInsert,
  BlobDBEventTypeDelete,
  BlobDBEventTypeFlush,
} BlobDBEventType;
