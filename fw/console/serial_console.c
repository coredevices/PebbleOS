/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "serial_console.h"

#include <pbl/kernel/irq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sched.h>
#include <pbl/mcu/interrupts.h>

#include <console/console_frame.h>
#include <console/dbgserial.h>
#include <console/dbgserial_input.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/backend.h>
#include <pbl/shell/shell.h>

extern const struct pbl_shell shell_uart;
#endif

static PBL_MUTEX_DEFINE(s_tx_lock);
static char s_last_char = '\n';

static bool prv_can_lock(void) {
  return !(pbl_irq_is_locked() || mcu_state_is_isr() || !pbl_kernel_is_running() ||
           pbl_sched_is_locked());
}

void serial_console_lock(void) {
  if (prv_can_lock()) {
    pbl_mutex_lock(&s_tx_lock, PBL_FOREVER);
  }
}

void serial_console_unlock(void) {
  if (prv_can_lock()) {
    pbl_mutex_unlock(&s_tx_lock);
  }
}

void serial_console_write_raw(const char *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    dbgserial_putchar(data[i]);
  }
  if (len > 0) {
    s_last_char = data[len - 1];
  }
}

void serial_console_write(const char *data, size_t len) {
  serial_console_lock();
  serial_console_write_raw(data, len);
  serial_console_unlock();
}

void serial_console_log_begin(void) {
  serial_console_lock();
#ifdef CONFIG_SHELL
  if (pbl_shell_at_prompt(&shell_uart)) {
    serial_console_write_raw("\r\033[K", 4);
    return;
  }
#endif
  if (s_last_char != '\n') {
    serial_console_write_raw("\r\n", 2);
  }
}

void serial_console_log_end(void) {
#ifdef CONFIG_SHELL
  if (pbl_shell_at_prompt(&shell_uart)) {
    pbl_shell_redraw(&shell_uart);
  }
#endif
  serial_console_unlock();
}

#ifdef CONFIG_SHELL
static void prv_shell_write(const struct pbl_shell *sh, const char *data, size_t len) {
  serial_console_write(data, len);
}

static void prv_shell_lock(const struct pbl_shell *sh) {
  serial_console_lock();
}

static void prv_shell_unlock(const struct pbl_shell *sh) {
  serial_console_unlock();
}

static const struct pbl_shell_backend_api s_shell_api = {
  .write = prv_shell_write,
  .lock = prv_shell_lock,
  .unlock = prv_shell_unlock,
};

PBL_SHELL_DEFINE(shell_uart, "pebble> ", &s_shell_api, nullptr);

#endif

static void prv_text_input(char c) {
#ifdef CONFIG_SHELL
  pbl_shell_input_from_isr(&shell_uart, c);
#endif
}

static void prv_rx(char c) {
  console_frame_input_from_isr(c, prv_text_input);
}

void serial_console_init(void) {
  dbgserial_input_init();
}

void serial_console_enable_prompt(void) {
  dbgserial_register_character_callback(prv_rx);
#ifdef CONFIG_SHELL
  pbl_shell_start(&shell_uart);
#endif
}

void serial_console_set_rx_enabled(bool enabled) {
  pbl_irq_lock();
  dbgserial_set_input_enabled(enabled);
  pbl_irq_unlock();
}
