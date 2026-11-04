/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! Start taking input, once the system can handle commands.
void serial_console_enable_prompt(void);

//! Hold other users of the port back, recursively. Does nothing in interrupt context, or with
//! interrupts or the scheduler locked.
void serial_console_lock(void);
void serial_console_unlock(void);

//! Write a character to the port without waiting for it to go out, without taking the lock.
void serial_console_putchar(char c);

//! Wait for what was written to go out.
void serial_console_flush(void);

//! Write to the port without taking the lock, for use in fault handlers and between
//! serial_console_log_begin() and serial_console_log_end().
void serial_console_write_raw(const char *data, size_t len);

//! Write to the port under the lock.
void serial_console_write(const char *data, size_t len);

//! Start a log line, moving the shell's input line out of the way.
void serial_console_log_begin(void);

//! End a log line, bringing the shell's input line back.
void serial_console_log_end(void);

void serial_console_set_rx_enabled(bool enabled);
