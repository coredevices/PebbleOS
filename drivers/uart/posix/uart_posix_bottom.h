/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Makes the host's terminal the console: raw, and read on a thread of its own.
void uart_posix_bottom_console_start(void);

//! A character was typed on the terminal. Called by the bottom, from its thread.
void uart_posix_console_rx(uint8_t c);
