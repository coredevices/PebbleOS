/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @file resource_storage_builtin.h

typedef struct {
  uint32_t resource_id;
  const uint8_t *address;
  uint32_t num_bytes;
} BuiltInResourceData;

bool resource_storage_builtin_bytes_are_readonly(const void *bytes);
