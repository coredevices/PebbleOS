/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "put_bytes_storage.h"

#include <stdbool.h>

bool pb_storage_raw_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                         uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset);

uint32_t pb_storage_raw_get_max_size(PutBytesObjectType object_type);

void pb_storage_raw_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                          uint32_t length);

uint32_t pb_storage_raw_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type);


void pb_storage_raw_deinit(PutBytesStorage *storage, bool is_success);


bool pb_storage_raw_get_status(PutBytesObjectType obj_type,  PbInstallStatus *status);
