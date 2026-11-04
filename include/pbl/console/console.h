/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

/**
 * @defgroup console Console
 * @ingroup subsys
 * @brief Plain text console on the board's console port: logs, the shell and framed binary data.
 * @{
 */

//! Start taking input, once the system can handle commands.
void pbl_console_enable_prompt(void);

//! Hold other users of the port back, recursively. Does nothing in interrupt context, or with
//! interrupts or the scheduler locked.
void pbl_console_lock(void);
void pbl_console_unlock(void);

//! Write a character to the port without waiting for it to go out, without taking the lock.
void pbl_console_putchar(char c);

//! Wait for what was written to go out.
void pbl_console_flush(void);

//! Write to the port without taking the lock, for use in fault handlers and between
//! pbl_console_log_begin() and pbl_console_log_end().
void pbl_console_write_raw(const char *data, size_t len);

//! Write to the port under the lock.
void pbl_console_write(const char *data, size_t len);

//! Start a log line, moving the shell's input line out of the way.
void pbl_console_log_begin(void);

//! End a log line, bringing the shell's input line back.
void pbl_console_log_end(void);

void pbl_console_set_rx_enabled(bool enabled);

/** @} */
