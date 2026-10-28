/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// clang-format off
#include "dbgserial_input.h"

#include <board/board.h>
#include <pbl/drivers/uart.h>
// clang-format on

#if !defined(CONFIG_RELEASE) || defined(CONFIG_MFG)

static DbgSerialCharacterCallback s_character_callback;

static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
  if (s_character_callback) {
    s_character_callback(c);
  }
}

void dbgserial_input_init(void) {
  (void)pbl_uart_rx_enable(DBG_UART, prv_rx, nullptr);
}

void dbgserial_register_character_callback(DbgSerialCharacterCallback callback) {
  s_character_callback = callback;
}

void dbgserial_set_input_enabled(bool enabled) {
  if (enabled) {
    (void)pbl_uart_rx_enable(DBG_UART, prv_rx, nullptr);
  } else {
    (void)pbl_uart_rx_disable(DBG_UART);
  }
}

#else
void dbgserial_input_init(void) {
}

void dbgserial_register_character_callback(DbgSerialCharacterCallback callback) {
}

void dbgserial_set_input_enabled(bool enabled) {
}
#endif
