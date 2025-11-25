/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/put_bytes/put_bytes.h"
#include "system/firmware_storage.h"

#include <stddef.h>
#include <stdint.h>

uint32_t fake_pb_storage_mem_get_max_size(PutBytesObjectType object_type);

void fake_pb_storage_mem_reset(void);

void fake_pb_storage_mem_set_crc(uint32_t crc);

bool fake_pb_storage_mem_get_last_success(void);

void fake_pb_storage_mem_assert_contents_written(const uint8_t contents[], size_t size);

void fake_pb_storage_mem_assert_fw_description_written(const FirmwareDescription *fw_descr);

void fake_pb_storage_register_cb_before_write(void (*cb_before_write)(void));
