/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <system/hexdump.h>

PBL_WEAK void hexdump_using_serial(int level, const char *src_filename, int src_line_number,
                                   const char *line_buffer) {
}

PBL_WEAK void hexdump_using_pbllog(int level, const char *src_filename, int src_line_number,
                                   const char *line_buffer) {
}

PBL_WEAK void hexdump_log(int level, const uint8_t *data, size_t length) {
}

PBL_WEAK void hexdump_log_src(const char *src_filename, int src_line_number, int level,
                              const uint8_t *data, size_t length, HexdumpLineCallback cb) {
}
