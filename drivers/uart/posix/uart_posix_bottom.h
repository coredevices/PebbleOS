/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Host end of a UART: the console is the terminal unless given a TCP port;
//! the QEMU one is a TCP port; Bluetooth HCI connects to a TCP port or opens a
//! serial device. Each is otherwise not connected.
enum uart_posix_channel {
  UART_POSIX_CONSOLE,
  UART_POSIX_QEMU,
  UART_POSIX_BT_HCI,
  UART_POSIX_NUM_CHANNELS,
};

void uart_posix_bottom_start(enum uart_posix_channel channel);

void uart_posix_bottom_write(enum uart_posix_channel channel, uint8_t c);

//! Whether the receive interrupt is enabled. A channel with flow control holds
//! its bytes until it is.
bool uart_posix_rx_enabled(enum uart_posix_channel channel);

//! A byte came in. Called by the bottom, from a thread of its own.
void uart_posix_rx(enum uart_posix_channel channel, uint8_t c);
