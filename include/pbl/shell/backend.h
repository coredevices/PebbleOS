/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/shell/shell.h>

/**
 * @defgroup shell_backend Shell backends
 * @ingroup shell
 * @brief Interface between the shell core and the transports that carry it.
 *
 * A backend owns one shell instance, defined with PBL_SHELL_DEFINE(), and moves its bytes.
 * Interactive backends (with a prompt) feed raw characters with pbl_shell_input_from_isr() and
 * get line editing, echo, the prompt and tab completion; line backends submit whole commands
 * with pbl_shell_execute_line() and are told when they finish.
 *
 * @code{.c}
 * static void prv_write(const struct pbl_shell *sh, const char *data, size_t len) {
 *   uart_write(data, len);
 * }
 *
 * static const struct pbl_shell_backend_api s_api = {
 *   .write = prv_write,
 * };
 *
 * PBL_SHELL_DEFINE(shell_uart, "pebble> ", &s_api, NULL);
 *
 * void uart_rx_isr(char c) {
 *   pbl_shell_input_from_isr(&shell_uart, c);
 * }
 * @endcode
 * @{
 */

/** @brief Backend operations. */
struct pbl_shell_backend_api {
  /** Writes output. Runs on the thread printing, usually KernelBG. */
  void (*write)(const struct pbl_shell *sh, const char *data, size_t len);
  /** Optional. A submitted command finished with @p ret. */
  void (*done)(const struct pbl_shell *sh, int ret);
};

/** @cond INTERNAL_HIDDEN */
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
/** @endcond */

/** @brief Shell instance, defined with PBL_SHELL_DEFINE(). */
struct pbl_shell {
  /** Instance name. */
  const char *name;
  /** Prompt of an interactive shell, NULL for a line shell. */
  const char *prompt;
  /** Backend operations. */
  const struct pbl_shell_backend_api *api;
  /** Backend data. */
  void *backend_data;
  /** Runtime state. */
  struct pbl_shell_ctx *ctx;
};

/**
 * @brief Define a shell instance and its state.
 *
 * @param _name Instance name, an identifier; the instance is a global <tt>const struct
 * pbl_shell</tt> with this name.
 * @param _prompt Prompt for an interactive shell, NULL for a line shell.
 * @param _api Backend operations.
 * @param _backend_data Backend data.
 */
#define PBL_SHELL_DEFINE(_name, _prompt, _api, _backend_data) \
  static struct pbl_shell_ctx _name##_ctx;                    \
  const struct pbl_shell _name = {                            \
    .name = #_name,                                           \
    .prompt = (_prompt),                                      \
    .api = (_api),                                            \
    .backend_data = (_backend_data),                          \
    .ctx = &_name##_ctx,                                      \
  }

/**
 * @brief Start a session of an interactive shell.
 *
 * Drops pending input; the prompt is printed from KernelBG. Call from an interrupt handler.
 *
 * @param sh Interactive shell.
 */
void pbl_shell_start_from_isr(const struct pbl_shell *sh);

/**
 * @brief End the session of an interactive shell, dropping pending input.
 *
 * @param sh Interactive shell.
 */
void pbl_shell_stop(const struct pbl_shell *sh);

/**
 * @brief Queue a character received by an interactive shell.
 *
 * Call from an interrupt handler. Characters that overflow @c CONFIG_SHELL_RX_BUFF_SIZE are
 * dropped.
 *
 * @param sh Interactive shell.
 * @param c Character.
 */
void pbl_shell_input_from_isr(const struct pbl_shell *sh, char c);

/**
 * @brief Run a command line on a line shell.
 *
 * The line is copied and runs on KernelBG; @ref pbl_shell_backend_api::done reports the result.
 *
 * @param sh Line shell.
 * @param line Command line, not NUL-terminated.
 * @param len Length of @p line.
 * @retval 0 The command was submitted.
 * @retval -EBUSY A command is running.
 * @retval -ENOSPC @p line is longer than @c CONFIG_SHELL_CMD_BUFF_SIZE.
 * @retval -ENOMEM The command could not be queued to KernelBG.
 */
int pbl_shell_execute_line(const struct pbl_shell *sh, const char *line, size_t len);

/**
 * @brief Check whether a command submitted to a shell is running.
 *
 * @param sh Shell.
 * @return true while a command runs.
 */
bool pbl_shell_is_busy(const struct pbl_shell *sh);

/** @} */
