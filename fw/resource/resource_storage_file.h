/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "resource_storage.h"

//! @file resource_storage_file.h

typedef struct {
  uint32_t first_resource_id;
  uint32_t last_resource_id;
  uint32_t resource_id_offset;
  const char *name;
} FileResourceData;

// TODO PBL-21009: Move this somewhere else.
#define APP_RESOURCE_FILENAME_MAX_LENGTH 24
