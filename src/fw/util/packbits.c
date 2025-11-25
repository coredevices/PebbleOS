/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "packbits.h"

#include <string.h>

void packbits_unpack(const char* src, int src_length, uint8_t* dest) {
  int length = 0;
  while (length < src_length) {
    int8_t header = *((int8_t*) src++);
    length++;

    if (header >= 0) {
      int count = header + 1;
      memcpy(dest, src, count);

      dest += count;
      src += count;
      length += count;
    } else {
      int count = 1 - header;
      memset(dest, *src, count);

      dest += count;
      src += 1;
      length += 1;
    }
  }
}
