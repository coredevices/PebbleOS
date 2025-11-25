/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dbgserial.h"

#include "board/board.h"
#include "drivers/uart.h"

#include <stdarg.h>
#include <stdio.h>


#if PULSE_EVERYWHERE
#define DEFAULT_SERIAL_BAUD_RATE 1000000
#else
#define DEFAULT_SERIAL_BAUD_RATE 230400
#endif


#if !defined(RELEASE) || defined(TARGET_QEMU) || defined(MANUFACTURING_FW)
void dbgserial_init(void) {
  uart_init(DBG_UART);
  dbgserial_restore_baud_rate();
}

void dbgserial_change_baud_rate(uint32_t new_baud) {
  uart_set_baud_rate(DBG_UART, new_baud);
}

void dbgserial_restore_baud_rate(void) {
  dbgserial_change_baud_rate(DEFAULT_SERIAL_BAUD_RATE);
}

void dbgserial_putstr(const char* str) {
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
  uart_write_byte(DBG_UART, c);
}

void dbgserial_flush(void) {
  uart_wait_for_tx_complete(DBG_UART);
}

void dbgserial_putstr_fmt(char* buffer, unsigned int buffer_size, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsniprintf(buffer, buffer_size, fmt, ap);
  va_end(ap);

  dbgserial_putstr(buffer);
}
#else
void dbgserial_init(void) {
}

void dbgserial_change_baud_rate(uint32_t new_baud) {
}

void dbgserial_restore_baud_rate(void) {
}

void dbgserial_putstr(const char* str) {
}

void dbgserial_putchar(uint8_t c) {
}

void dbgserial_putchar_lazy(uint8_t c) {
}

void dbgserial_flush(void) {
}

void dbgserial_putstr_fmt(char* buffer, unsigned int buffer_size, const char* fmt, ...) {
}
#endif
