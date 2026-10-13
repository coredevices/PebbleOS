/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include <stdint.h>

void stub_log_internal(uint8_t log_level, const char *src_filename, int src_line_number,
                       const char *fmt, va_list args);
