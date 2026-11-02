/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include <pbl/logging/logging.h>

// offset + gap + 8 hex bytes + gap + 8 hex bytes + gap + 8 ascii bytes
// + mini gap + 8 ascii bytes + null:
#define LINE_BUFFER_LENGTH (4 + 2 + (3 * 8) + 2 + (3 * 8) + 2 + 8 + 1 + 8 + 1)

void pbl_log_hexdump(uint8_t log_level, const char *src_filename, int src_line_number,
                     const void *data, size_t length) {
  const uint8_t *bytes = data;
  char line_buffer[LINE_BUFFER_LENGTH];
  unsigned int offset = 0;

  while (offset < length) {
    unsigned int buffer_offset = 0;

    snprintf(line_buffer, LINE_BUFFER_LENGTH, "%04x  ", offset);
    buffer_offset += 6;

    const unsigned int num_line_bytes = ((length - offset) > 16) ? 16 : (length - offset);
    for (unsigned int i = 0; i < num_line_bytes; ++i) {
      if (i == 8) {
        line_buffer[buffer_offset++] = ' ';
      }

      snprintf(line_buffer + buffer_offset, LINE_BUFFER_LENGTH - buffer_offset, "%02x ",
               bytes[offset + i]);
      buffer_offset += 3;
    }

    // Pad partial lines so the ASCII column lines up.
    unsigned int required_padding = 2;
    if (num_line_bytes < 16) {
      required_padding += (16 - num_line_bytes) * 3;
      if (num_line_bytes <= 8) {
        required_padding += 1;
      }
    }

    memset(line_buffer + buffer_offset, ' ', required_padding);
    buffer_offset += required_padding;

    for (unsigned int i = 0; i < num_line_bytes; ++i) {
      if (i == 8) {
        line_buffer[buffer_offset++] = ' ';
      }

      char c = bytes[offset + i];
      // ` delimits log hash strings
      if (c < 32 || c > 126 || c == '`') {
        c = '.';
      }
      line_buffer[buffer_offset++] = c;
    }

    line_buffer[buffer_offset] = 0;
    pbl_log_sync(log_level, src_filename, src_line_number, "%s", line_buffer);

    offset += 16;
  }
}
