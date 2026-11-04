/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
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

static int prv_decode_char(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}

int pbl_base64_decode(uint8_t *out, size_t out_len, const char *data, size_t data_len) {
  size_t written = 0U;

  if (data_len % 4U != 0U) {
    return -EINVAL;
  }

  for (size_t i = 0U; i < data_len; i += 4U) {
    const bool last = (i + 4U == data_len);
    const size_t padding = (last && data[i + 3] == '=') ? ((data[i + 2] == '=') ? 2U : 1U) : 0U;
    const size_t bytes = 3U - padding;
    uint32_t group = 0U;

    for (size_t j = 0U; j < 4U - padding; j++) {
      const int value = prv_decode_char(data[i + j]);
      if (value < 0) {
        return -EINVAL;
      }
      group |= (uint32_t)value << (18U - 6U * j);
    }

    if (written + bytes > out_len) {
      return -ENOSPC;
    }
    for (size_t j = 0U; j < bytes; j++) {
      out[written++] = (uint8_t)(group >> (16U - 8U * j));
    }
  }

  return (int)written;
}
