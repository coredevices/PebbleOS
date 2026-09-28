/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pbl/shell/shell.h"

//! Shell backend interface. A backend owns one shell instance and moves its
//! bytes: interactive backends feed raw characters and get line editing,
//! echo, a prompt and tab completion; line backends submit whole commands.

struct pbl_shell_backend_api {
  //! Writes output. Runs on the thread printing, usually KernelBG.
  void (*write)(const struct pbl_shell *sh, const char *data, size_t len);
  //! Optional. A submitted command finished with @p ret.
  void (*done)(const struct pbl_shell *sh, int ret);
};

struct pbl_shell_ctx {
  char line[CONFIG_SHELL_CMD_BUFF_SIZE + 1];
  uint16_t len;
  uint8_t esc;
  bool last_cr;
  bool busy;
  bool active;
  volatile bool rx_scheduled;
  volatile uint8_t rx_head;
  volatile uint8_t rx_tail;
  char rx[CONFIG_SHELL_RX_BUFF_SIZE];
};

struct pbl_shell {
  const char *name;
  //! Prompt of an interactive shell, NULL for a line shell.
  const char *prompt;
  const struct pbl_shell_backend_api *api;
  void *backend_data;
  struct pbl_shell_ctx *ctx;
};

#define PBL_SHELL_DEFINE(_name, _prompt, _api, _backend_data) \
  static struct pbl_shell_ctx _name##_ctx;                    \
  const struct pbl_shell _name = {                            \
    .name = #_name,                                           \
    .prompt = (_prompt),                                      \
    .api = (_api),                                            \
    .backend_data = (_backend_data),                          \
    .ctx = &_name##_ctx,                                      \
  }

//! Interactive shells: starts a session, printing the prompt from KernelBG.
void pbl_shell_start_from_isr(const struct pbl_shell *sh, bool *should_context_switch);

//! Interactive shells: ends the session, dropping pending input.
void pbl_shell_stop(const struct pbl_shell *sh);

//! Interactive shells: queues a received character. ISR-safe; characters
//! overflowing CONFIG_SHELL_RX_BUFF_SIZE are dropped.
void pbl_shell_input_from_isr(const struct pbl_shell *sh, char c, bool *should_context_switch);

//! Line shells: runs @p line (not NUL-terminated) on KernelBG.
//! @return 0, -EBUSY while a command runs, or -ENOSPC for a line longer
//! than CONFIG_SHELL_CMD_BUFF_SIZE.
int pbl_shell_execute_line(const struct pbl_shell *sh, const char *line, size_t len);

//! @return true while a command submitted to @p sh runs.
bool pbl_shell_is_busy(const struct pbl_shell *sh);
