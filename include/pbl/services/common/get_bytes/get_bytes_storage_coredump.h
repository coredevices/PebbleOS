/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "get_bytes_storage.h"

bool gb_storage_coredump_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                              GetBytesStorageInfo *info);

GetBytesInfoErrorCode gb_storage_coredump_get_size(GetBytesStorage *storage, uint32_t *size);

bool gb_storage_coredump_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len);

void gb_storage_coredump_cleanup(GetBytesStorage *storage, bool successful);
