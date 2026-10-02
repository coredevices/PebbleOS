/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

/**
 * @defgroup util_base64 Base64
 * @ingroup util
 * @brief Base64 encoding.
 * @{
 */

/**
 * @brief Encode data as standard (RFC 4648) padded base64.
 *
 * Calling it with a too small @p out (e.g. @p out_len 0) returns the required length without
 * writing anything.
 *
 * @param[out] out Output buffer. Written only if it can hold the whole encoding; NUL-terminated
 * when there is room left for the terminator.
 * @param out_len Size of @p out in bytes.
 * @param data Data to encode.
 * @param data_len Number of bytes to encode.
 * @return Length of the encoding, excluding the NUL terminator.
 */
size_t pbl_base64_encode(char *out, size_t out_len, const void *data, size_t data_len);

/** @} */
