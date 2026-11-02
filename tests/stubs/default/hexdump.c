/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>

PBL_WEAK void pbl_log_hexdump(uint8_t log_level, const char *src_filename, int src_line_number,
                              const void *data, size_t length) {
}
