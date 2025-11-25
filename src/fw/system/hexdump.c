/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/hexdump.h"
#include "system/logging.h"
#include "util/hexdump.h"
#include "console/prompt.h"

void hexdump_log(int level, const uint8_t *data, size_t length) {
  PBL_HEXDUMP_D(LOG_DOMAIN_MISC, level, data, length);
}

void hexdump_using_serial(int level, const char *src_filename, int src_line_number,
                          const char *line_buffer) {
  dbgserial_putstr(line_buffer);
}

void hexdump_using_prompt(int level, const char *src_filename, int src_line_number,
                          const char *line_buffer) {
  prompt_send_response(line_buffer);
}

void hexdump_using_pbllog(int level, const char *src_filename, int src_line_number,
                          const char *line_buffer) {
  pbl_log_sync(level, src_filename, src_line_number, "%s", line_buffer);
}

void hexdump_log_src(const char *src_filename, int src_line_number, int level,
                     const uint8_t *data, size_t length, HexdumpLineCallback cb) {
  hexdump(src_filename, src_line_number, level, data, length, cb);
}
