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

void dbgserial_putstr_fmt(char* buffer, unsigned int buffer_size, const char* fmt, ...)
    __attribute__((__format__(__printf__, 3, 4)));

void dbgserial_change_baud_rate(uint32_t new_baud);

//! Restore the dbgserial baud rate to the default (e.g. after a call to
//! dbgserial_change_baud_rate).
void dbgserial_restore_baud_rate(void);

//! Finish writing all characters to dbgserial output.
void dbgserial_flush(void);
