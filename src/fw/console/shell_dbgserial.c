/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/shell/backend.h>
#include <pbl/shell/shell.h>

#include "console/console_internal.h"
#include "console/dbgserial.h"
#include "console/shell_dbgserial.h"

static void prv_write(const struct pbl_shell *sh, const char *data, size_t len) {
  if (serial_console_get_state() == SERIAL_CONSOLE_STATE_PULSE) {
    return;
  }

  for (size_t i = 0; i < len; i++) {
    dbgserial_putchar(data[i]);
  }
}

static const struct pbl_shell_backend_api s_api = {
  .write = prv_write,
};

PBL_SHELL_DEFINE(shell_dbgserial, "pebble> ", &s_api, NULL);

void shell_dbgserial_start_from_isr(void) {
  serial_console_set_state(SERIAL_CONSOLE_STATE_PROMPT);
  pbl_shell_start_from_isr(&shell_dbgserial);
}

void shell_dbgserial_handle_char(char c) {
  if (c == 0x04) {
    pbl_shell_stop(&shell_dbgserial);
    dbgserial_putstr("^D");
    serial_console_set_state(SERIAL_CONSOLE_STATE_LOGGING);
    return;
  }

  pbl_shell_input_from_isr(&shell_dbgserial, c);
}
