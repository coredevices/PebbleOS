/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/util/string.h>

PBL_WEAK bool g_pbl_log_enabled = false;
PBL_WEAK int g_pbl_log_level = 0;

PBL_WEAK void stub_log_internal(uint8_t log_level, const char *src_filename, int src_line_number,
                                const char *fmt, va_list args) {
  printf("%s:%d> ", GET_FILE_NAME(src_filename), src_line_number);
  vprintf(fmt, args);
  printf("\n");
}

PBL_WEAK void pbl_log_vargs(uint8_t log_level, const char *src_filename, int src_line_number,
                            const char *fmt, va_list args) {
  stub_log_internal(log_level, src_filename, src_line_number, fmt, args);
}

PBL_WEAK void pbl_log(uint8_t log_level, const char *src_filename, int src_line_number,
                      const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  stub_log_internal(log_level, src_filename, src_line_number, fmt, args);
  va_end(args);
}

PBL_WEAK void pbl_log_sync(uint8_t log_level, const char *src_filename, int src_line_number,
                           const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  stub_log_internal(log_level, src_filename, src_line_number, fmt, args);
  va_end(args);
}

PBL_WEAK void command_dump_malloc() {
}

PBL_WEAK void reset_due_to_software_failure() {
  assert(0);
}

PBL_WEAK void app_log_vargs(uint8_t log_level, const char *src_filename, int src_line_number,
                            const char *fmt, va_list args) {
  stub_log_internal(log_level, src_filename, src_line_number, fmt, args);
}

PBL_WEAK void app_log(uint8_t log_level, const char *src_filename, int src_line_number,
                      const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  stub_log_internal(log_level, src_filename, src_line_number, fmt, args);
  va_end(args);
}

PBL_WEAK void kernel_pbl_log_from_fault_handler_fmt(const char *src_filename,
                                                    uint16_t src_line_number, char *buffer,
                                                    unsigned int buffer_size, const char *fmt,
                                                    ...) {
  va_list args;
  va_start(args, fmt);
  stub_log_internal(LOG_LEVEL_ALWAYS, src_filename, src_line_number, fmt, args);
  va_end(args);
}
