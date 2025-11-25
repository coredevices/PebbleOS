/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Configure the micro's peripherals to communicate with the flash chip
void flash_init(void);

//! Read 1 or more bytes starting at the specified 24bit address into
//! the provided buffer. This function does no range checking, so it is
//! currently possible to run off the end of the flash.
//!
//! @param buffer A byte-buffer that will be used to store the data
//! read from flash.
//! @param start_addr The address of the first byte to be read from flash.
//! @param buffer_size The total number of bytes to be read from flash.
void flash_read_bytes(uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size);

//! Check if we can talk to the flash.
//! @return true if the CFI table can be queried.
bool flash_sanity_check(void);

//! Get the checksum of a region of flash
uint32_t flash_calculate_checksum(uint32_t flash_addr, uint32_t length);
