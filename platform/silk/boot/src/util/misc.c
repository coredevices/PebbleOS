/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "misc.h"

#include "drivers/dbgserial.h"

#include <stdint.h>

void itoa_hex(uint32_t num, char *buffer, int buffer_length) {
  if (buffer_length < 11) {
    dbgserial_putstr("itoa buffer too small");
    return;
  }
  *buffer++ = '0';
  *buffer++ = 'x';

  for (int i = 7; i >= 0; --i) {
    uint32_t digit = (num & (0xf << (i * 4))) >> (i * 4);

    char c;
    if (digit < 0xa) {
      c = '0' + digit;
    } else if (digit < 0x10) {
      c = 'a' + (digit - 0xa);
    } else {
      c = ' ';
    }

    *buffer++ = c;
  }
  *buffer = '\0';
}
