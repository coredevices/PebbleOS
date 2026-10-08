/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/util/base64.h>

static const char s_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t pbl_base64_encode(char *out, size_t out_len, const void *data, size_t data_len) {
  const uint8_t *in = data;
  const size_t result = (data_len + 2) / 3 * 4;
  if (result > out_len) {
    return result;
  }

  size_t i;
  for (i = 0; i + 2 < data_len; i += 3) {
    *out++ = s_alphabet[in[i] >> 2];
    *out++ = s_alphabet[((in[i] & 0x03) << 4) | (in[i + 1] >> 4)];
    *out++ = s_alphabet[((in[i + 1] & 0x0f) << 2) | (in[i + 2] >> 6)];
    *out++ = s_alphabet[in[i + 2] & 0x3f];
  }

  if (data_len - i == 2) {
    *out++ = s_alphabet[in[i] >> 2];
    *out++ = s_alphabet[((in[i] & 0x03) << 4) | (in[i + 1] >> 4)];
    *out++ = s_alphabet[(in[i + 1] & 0x0f) << 2];
    *out++ = '=';
  } else if (data_len - i == 1) {
    *out++ = s_alphabet[in[i] >> 2];
    *out++ = s_alphabet[(in[i] & 0x03) << 4];
    *out++ = '=';
    *out++ = '=';
  }

  if (result < out_len) {
    *out = '\0';
  }
  return result;
}
