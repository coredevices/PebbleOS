/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! Encodes data as standard (RFC 4648) padded base64.
//! @param out Output buffer. Written only if it can hold the whole encoding; NUL-terminated
//!   when there is room left for the terminator.
//! @param out_len Size of the output buffer
//! @param data Data to encode
//! @param data_len Number of bytes to encode
//! @return Length of the encoding, excluding the NUL terminator
size_t pbl_base64_encode(char *out, size_t out_len, const void *data, size_t data_len);
