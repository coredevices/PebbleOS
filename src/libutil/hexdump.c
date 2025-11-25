/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/hexdump.h"

#include <stdio.h>
#include <string.h>

// offset + gap + 8 hex bytes + gap + 8 hex bytes + gap + 8 ascii bytes
// + mini gap + 8 ascii bytes + null:
#define LINE_BUFFER_LENGTH (4 + 2 + (3 * 8) + 2 + (3 * 8) + 2 + 8 + 1 + 8 + 1)

void hexdump(const char *src_filename, int src_line_number, int level,
             const uint8_t *data, size_t length, HexdumpLineCallback write_line_cb) {
  char line_buffer[LINE_BUFFER_LENGTH];
  unsigned int offset = 0;

  while (offset < length) {
    unsigned int buffer_offset = 0;

    // Print data line offset
    snprintf(line_buffer, LINE_BUFFER_LENGTH, "%04x  ", offset);
    buffer_offset += 6;

    // Print the hex bytes
    const unsigned int num_line_bytes = ((length - offset) > 16) ? 16 : (length - offset);
    for (unsigned int i = 0; i < num_line_bytes; ++i) {
      if (i == 8) {
        line_buffer[buffer_offset++] = ' ';
      }

      snprintf(line_buffer + buffer_offset, LINE_BUFFER_LENGTH - buffer_offset, "%02x ",
               data[offset + i]);
      buffer_offset += 3;
    }

    // Calculate and apply padding between the hex dump and the ascii dump.
    unsigned int required_padding = 2;
    if (num_line_bytes < 16) {
      // If we're printing a partial line, pad out the rest so the
      // ascii lines up.
      required_padding += (16 - num_line_bytes) * 3;
      if (num_line_bytes <= 8)
      {
        // Account for the gap between the 8 byte hex blocks.
        required_padding += 1;
      }
    }

    memset(line_buffer + buffer_offset, ' ', required_padding);
    buffer_offset += required_padding;

    // Print the ASCII bytes
    for (unsigned int i = 0; i < num_line_bytes; ++i) {
      if (i == 8)
      {
        line_buffer[buffer_offset++] = ' ';
      }

      char c = data[offset + i];
      if (c < 32 || c > 126 || c == '`') // ` is used for log hash string delimiting
      {
        c = '.';
      }
      line_buffer[buffer_offset++] = c;
    }

    // No need to pad after a partial ascii line, since we don't line up
    // anything after it.

    // Null terminate and print.
    line_buffer[buffer_offset] = 0;
    write_line_cb(level, src_filename, src_line_number, line_buffer);

    offset += 16;
  }
}
