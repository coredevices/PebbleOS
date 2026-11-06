/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Console port.
extern const struct pbl_uart *const CONSOLE_UART;

void board_early_init(void);
void board_init(void);

// FIXME: remove once the HRM moves to the device model
typedef const struct HRMDevice HRMDevice;

#ifdef CONFIG_BOARD_HEADER
#include CONFIG_BOARD_HEADER
#endif
