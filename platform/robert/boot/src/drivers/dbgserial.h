/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include <stdint.h>

void dbgserial_init(void);

void dbgserial_putstr(const char* str);

void dbgserial_newline(void);

//! Like dbgserial_putstr, but without a terminating newline
void dbgserial_print(const char* str);

void dbgserial_print_hex(uint32_t value);

void dbgserial_putstr_fmt(char* buffer, unsigned int buffer_size, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
