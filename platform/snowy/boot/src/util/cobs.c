/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "cobs.h"

size_t cobs_encode(void *dst_ptr, const void *src_ptr, size_t length) {
  const char *src = src_ptr;
  char *dst = dst_ptr;
  uint8_t code = 0x01;
  size_t code_idx = 0;
  size_t dst_idx = 1;

  for (size_t src_idx = 0; src_idx < length; ++src_idx) {
    if (src[src_idx] == '\0') {
      dst[code_idx] = code;
      code_idx = dst_idx++;
      code = 0x01;
    } else {
      dst[dst_idx++] = src[src_idx];
      code++;
      if (code == 0xff) {
        if (src_idx == length - 1) {
          // Special case: the final encoded block is 254 bytes long with no
          // zero after it. While it's technically a valid encoding if a
          // trailing zero is appended, it causes the output to be one byte
          // longer than it needs to be. This violates consistent overhead
          // contract and could overflow a carefully sized buffer.
          break;
        }
        dst[code_idx] = code;
        code_idx = dst_idx++;
        code = 0x01;
      }
    }
  }
  dst[code_idx] = code;
  return dst_idx;
}
