/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Host end of a UART: the console is the terminal unless given a TCP port;
//! the others are a TCP port, or not connected.
enum uart_posix_channel {
  UART_POSIX_CONSOLE,
  UART_POSIX_QEMU,
  UART_POSIX_NUM_CHANNELS,
};

void uart_posix_bottom_start(enum uart_posix_channel channel);

void uart_posix_bottom_write(enum uart_posix_channel channel, uint8_t c);

//! A byte came in. Called by the bottom, from a thread of its own.
void uart_posix_rx(enum uart_posix_channel channel, uint8_t c);
