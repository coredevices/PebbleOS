/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// clang-format off
#include "dbgserial.h"

#include <stdarg.h>
#include <stdio.h>


#include <board/board.h>
#include <pbl/drivers/uart.h>
// clang-format on

#ifdef CONFIG_PULSE_EVERYWHERE
#define DEFAULT_SERIAL_BAUD_RATE 1000000
#else
#define DEFAULT_SERIAL_BAUD_RATE 115200
#endif

// The port is an early console until its device init takes over
void dbgserial_init(void) {
  const struct pbl_uart_config cfg = {.baudrate = DEFAULT_SERIAL_BAUD_RATE};

  (void)pbl_uart_early_init(DBG_UART, &cfg);
}

void dbgserial_change_baud_rate(uint32_t new_baud) {
  const struct pbl_uart_config cfg = {.baudrate = new_baud};

  (void)pbl_uart_configure(DBG_UART, &cfg);
}

void dbgserial_restore_baud_rate(void) {
  dbgserial_change_baud_rate(DEFAULT_SERIAL_BAUD_RATE);
}

void dbgserial_putstr(const char *str) {
  while (*str) {
    dbgserial_putchar(*str);
    ++str;
  }
  dbgserial_putchar('\r');
  dbgserial_putchar('\n');
}

void dbgserial_putchar(uint8_t c) {
  dbgserial_putchar_lazy(c);
  dbgserial_flush();
}

void dbgserial_putchar_lazy(uint8_t c) {
  pbl_uart_poll_out(DBG_UART, c);
}

void dbgserial_flush(void) {
  pbl_uart_flush(DBG_UART);
}

void dbgserial_putstr_fmt(char *buffer, unsigned int buffer_size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsniprintf(buffer, buffer_size, fmt, ap);
  va_end(ap);

  dbgserial_putstr(buffer);
}
