/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pbl/kernel/compiler.h"

//! Shell: commands registered from anywhere in the tree, executed on
//! KernelBG for any number of shell instances, one per backend. See
//! docs/architecture/shell.md.

struct pbl_shell;

//! @param argv the matched command name followed by its arguments.
//! @return 0 on success, a negative errno on failure, or -EINPROGRESS to
//! finish later with pbl_shell_cmd_done().
typedef int (*pbl_shell_cmd_handler_t)(const struct pbl_shell *sh, size_t argc, char **argv);

struct pbl_shell_cmd {
  const char *syntax;
  const char *help;
  const struct pbl_shell_cmd *subcmd;
  pbl_shell_cmd_handler_t handler;
  //! Arguments counting the command name itself; 0 skips the check.
  uint8_t mandatory;
  //! Extra arguments allowed, or PBL_SHELL_OPT_ARG_MAX for any number.
  uint8_t optional;
};

#define PBL_SHELL_OPT_ARG_MAX UINT8_MAX

#define PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt) \
  {                                                                       \
    .syntax = #_syntax,                                                   \
    .help = (_help),                                                      \
    .subcmd = (_subcmd),                                                  \
    .handler = (_handler),                                                \
    .mandatory = (_mand),                                                 \
    .optional = (_opt),                                                   \
  }

#define PBL_SHELL_CMD(_syntax, _subcmd, _help, _handler) \
  PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, 0, 0)

//! Terminates a file-local subcommand array of PBL_SHELL_CMD() entries.
#define PBL_SHELL_SUBCMD_SET_END {0}

//! Defines subcommand set _name that any file can extend with
//! PBL_SHELL_SUBCMD_ADD(). Entries are sorted by name at link time.
#define PBL_SHELL_SUBCMD_SET_CREATE(_name)                              \
  static const struct pbl_shell_cmd _name[0] PBL_USED PBL_ALIGNED(4)    \
      PBL_SECTION(".pbl_shell_subcmds." #_name ".!");                   \
  static const struct pbl_shell_cmd _name##_end PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_shell_subcmds." #_name ".~") = PBL_SHELL_SUBCMD_SET_END

#define PBL_SHELL_SUBCMD_ADD_IMPL(_id, _set, _syntax, _subcmd, _help, _handler, _mand, _opt) \
  static const struct pbl_shell_cmd pbl_shell_subcmd_##_id PBL_USED PBL_ALIGNED(4)           \
      PBL_SECTION(".pbl_shell_subcmds." #_set "." #_syntax) =                                \
          PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt)

#define PBL_SHELL_SUBCMD_ADD_ID(_id, ...) PBL_SHELL_SUBCMD_ADD_IMPL(_id, __VA_ARGS__)

//! Adds a subcommand to the set created with PBL_SHELL_SUBCMD_SET_CREATE(_set),
//! from any file. Entries of a set that is not built are unreachable.
#define PBL_SHELL_SUBCMD_ADD(_set, _syntax, _subcmd, _help, _handler, _mand, _opt) \
  PBL_SHELL_SUBCMD_ADD_ID(__COUNTER__, _set, _syntax, _subcmd, _help, _handler, _mand, _opt)

//! Registers a root command. Root commands are sorted by name at link time.
#define PBL_SHELL_CMD_ARG_REGISTER(_syntax, _subcmd, _help, _handler, _mand, _opt)       \
  static const struct pbl_shell_cmd pbl_shell_root_cmd_##_syntax PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_shell_root_cmds." #_syntax) =                                    \
          PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt)

#define PBL_SHELL_CMD_REGISTER(_syntax, _subcmd, _help, _handler) \
  PBL_SHELL_CMD_ARG_REGISTER(_syntax, _subcmd, _help, _handler, 0, 0)

//! Prints a formatted line.
void pbl_shell_print(const struct pbl_shell *sh, const char *fmt, ...) PBL_FORMAT_PRINTF(2, 3);

//! Prints formatted text without appending a line break.
void pbl_shell_fprintf(const struct pbl_shell *sh, const char *fmt, ...) PBL_FORMAT_PRINTF(2, 3);

void pbl_shell_vfprintf(const struct pbl_shell *sh, const char *fmt, va_list args);

//! Prints a formatted error line.
void pbl_shell_error(const struct pbl_shell *sh, const char *fmt, ...) PBL_FORMAT_PRINTF(2, 3);

//! Prints a hex dump of @p data, 16 bytes a line.
void pbl_shell_hexdump(const struct pbl_shell *sh, const void *data, size_t len);

//! Prints the help of @p cmd and its subcommands.
void pbl_shell_help(const struct pbl_shell *sh, const struct pbl_shell_cmd *cmd);

//! Finishes a command whose handler returned -EINPROGRESS. Any thread.
void pbl_shell_cmd_done(const struct pbl_shell *sh, int ret);

//! Parses a string argument as a signed integer (decimal, 0x or 0 prefixes).
//! @return 0, or -EINVAL if @p str is not a number that fits.
int pbl_shell_strtol(const char *str, long *out);

//! As pbl_shell_strtol(), for unsigned values.
int pbl_shell_strtoul(const char *str, unsigned long *out);
