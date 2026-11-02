/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/logging/backend_flash.h>

// The log region must span at least two erase units, aligned on erase unit boundaries.

#define PBL_LOG__FLASH_INVALID_ADDR UINT32_MAX

void pbl_log__flash_storage_init(void);

//! Find space for a message of @p msg_length bytes and write its record header.
//!
//! @return Flash address to write the message at, or PBL_LOG__FLASH_INVALID_ADDR.
uint32_t pbl_log__flash_storage_log_start(uint8_t msg_length);

//! Write (part of) a message started with pbl_log__flash_storage_log_start().
bool pbl_log__flash_storage_write(const uint8_t *data_to_write, uint32_t flash_addr,
                                  uint32_t data_length);
