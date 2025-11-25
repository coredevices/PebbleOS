/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdint.h>

unsigned int base64_decode_inplace(char* buffer, unsigned int length);

// Encode a buffer as base 64
// @param out the encoded base64 string is written here
// @param out_len the size of the out buffer
// @param data the binary data to encode
// @param data_len the number of bytes of binary data
// @retval the number of characters required to encode all of the data, not including the
//         terminating null at the end. If this is less than the passed in out_len, then
//         NO characters will be written to the out buffer.
int32_t base64_encode(char *out, int out_len, const uint8_t *data, int32_t data_len);
