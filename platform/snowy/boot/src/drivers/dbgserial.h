/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

void dbgserial_init(void);

void dbgserial_putchar(uint8_t c);

//! Version of dbgserial_putchar that may return before the character is finished writing.
//! Use if you don't need a guarantee that your character will be written.
void dbgserial_putchar_lazy(uint8_t c);

void dbgserial_putstr(const char* str);

//! Like dbgserial_putstr, but without a terminating newline
void dbgserial_print(const char* str);

void dbgserial_print_hex(uint32_t value);

void dbgserial_putstr_fmt(char* buffer, unsigned int buffer_size, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
