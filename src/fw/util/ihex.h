/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Intel HEX utilities

#pragma once

#include <stdint.h>


#define IHEX_TYPE_DATA (0u)
#define IHEX_TYPE_EOF (1u)


#define IHEX_RECORD_LENGTH(len) ((len)*2 + 11)


//! Encode an Intel HEX record with the specified record type, address
//! and data, and write it to out.
//!
//! \param [out] out destination buffer. Must be at least
//!              IHEX_RECORD_LENGTH(data_len) bytes long.
//! \param type record type
//! \param address record address
//! \param [in] data data for the record. May be NULL if data_len is 0.
//! \param data_len length of data to include in the record.
void ihex_encode(uint8_t *out, uint8_t type, uint16_t address,
                 const void *data, uint8_t data_len);
