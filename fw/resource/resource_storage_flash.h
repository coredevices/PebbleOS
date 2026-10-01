/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "resource_storage.h"

//! @file resource_storage_flash.h
//!
//! Functions for flash-based resource storage implementations

typedef struct SystemResourceBank {
  uint32_t begin;
  uint32_t end;
} SystemResourceBank;

//! Get the extents of a resource storage bank which is not currently in use by
//! the system.
// TODO PBL-21009: Move this somewhere else.
const SystemResourceBank *resource_storage_flash_get_unused_bank(void);

bool resource_storage_flash_bytes_are_readonly(const void *bytes);
