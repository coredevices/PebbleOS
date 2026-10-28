/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Console port.
extern const struct pbl_uart *const CONSOLE_UART;

#ifdef CONFIG_BOARD_HEADER
#include CONFIG_BOARD_HEADER
#endif
