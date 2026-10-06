/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pbl/kernel/compiler.h"
#include "pbl/kernel/section.h"

/**
 * @defgroup shell Shell
 * @ingroup subsys
 * @brief Debug command shell (@c CONFIG_SHELL).
 *
 * Commands are defined next to the code they drive, collected at link time and served by any
 * number of shell instances, one per backend (see @ref shell_backend). Commands form a tree:
 * a command has a name, a help string, an optional handler and optional subcommands. Handlers
 * of every instance run on KernelBG.
 *
 * A handler gets @c argc / @c argv with @c argv[0] being the matched command name. The core
 * checks the argument count, prints the help of the matched command for @c -h / @c --help,
 * unknown subcommands and commands without a handler, and prints an error for unknown commands.
 *
 * @code{.c}
 * static int prv_accel_read(const struct pbl_shell *sh, size_t argc, char **argv) {
 *   ...
 *   pbl_shell_print(sh, "x=%d y=%d z=%d mg", sample.x, sample.y, sample.z);
 *   return 0;
 * }
 *
 * static const struct pbl_shell_cmd sub_accel[] = {
 *   PBL_SHELL_CMD(read, NULL, "Read one sample", prv_accel_read),
 *   PBL_SHELL_SUBCMD_SET_END,
 * };
 *
 * PBL_SHELL_CMD_REGISTER(accel, sub_accel, "Accelerometer", NULL);
 * @endcode
 *
 * Code in other files can extend a command when its subcommands are a set:
 *
 * @code{.c}
 * // owner
 * PBL_SHELL_SUBCMD_SET_CREATE(sub_flash);
 * PBL_SHELL_CMD_REGISTER(flash, sub_flash, "Flash", NULL);
 *
 * // any other file
 * PBL_SHELL_SUBCMD_ADD(sub_flash, erase, NULL, "Erase <addr> <len>", prv_erase, 3, 0);
 * @endcode
 * @{
 */

struct pbl_shell;

/**
 * @brief Command handler.
 *
 * @param sh Shell the command runs on; its output goes back there.
 * @param argc Number of arguments in @p argv, the command name included.
 * @param argv Matched command name followed by its arguments, NULL-terminated.
 * @return 0 on success, a negative errno on failure, or -EINPROGRESS to finish later with
 * pbl_shell_cmd_done().
 */
typedef int (*pbl_shell_cmd_handler_t)(const struct pbl_shell *sh, size_t argc, char **argv);

/** @brief Command. */
struct pbl_shell_cmd {
  /** Name. */
  const char *syntax;
  /** Help string, or NULL. */
  const char *help;
  /** Subcommands, terminated by @ref PBL_SHELL_SUBCMD_SET_END, or NULL. */
  const struct pbl_shell_cmd *subcmd;
  /** Handler, or NULL to print the help. */
  pbl_shell_cmd_handler_t handler;
  /** Arguments counting the command name itself; 0 skips the check. */
  uint8_t mandatory;
  /** Extra arguments allowed, or @ref PBL_SHELL_OPT_ARG_MAX for any number. */
  uint8_t optional;
};

/** @brief Any number of optional arguments. */
#define PBL_SHELL_OPT_ARG_MAX UINT8_MAX

/**
 * @brief Initializer of a command with an argument count check.
 *
 * @param _syntax Name, an identifier.
 * @param _subcmd Subcommands, or NULL.
 * @param _help Help string, or NULL.
 * @param _handler Handler, or NULL.
 * @param _mand Mandatory arguments, the command name included; 0 skips the check.
 * @param _opt Optional arguments, or @ref PBL_SHELL_OPT_ARG_MAX.
 */
#define PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt) \
  {                                                                       \
    .syntax = #_syntax,                                                   \
    .help = (_help),                                                      \
    .subcmd = (_subcmd),                                                  \
    .handler = (_handler),                                                \
    .mandatory = (_mand),                                                 \
    .optional = (_opt),                                                   \
  }

/**
 * @brief Initializer of a command without an argument count check.
 *
 * @param _syntax Name, an identifier.
 * @param _subcmd Subcommands, or NULL.
 * @param _help Help string, or NULL.
 * @param _handler Handler, or NULL.
 */
#define PBL_SHELL_CMD(_syntax, _subcmd, _help, _handler) \
  PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, 0, 0)

/** @brief Terminates a file-local subcommand array of PBL_SHELL_CMD() entries. */
#define PBL_SHELL_SUBCMD_SET_END {0}

/**
 * @brief Define a subcommand set that any file can extend with PBL_SHELL_SUBCMD_ADD().
 *
 * Entries are sorted by name by the linker script, or when looked up in builds without one
 * (@c PBL_NO_LINKER_SCRIPT).
 *
 * @param _name Name of the set, usable as the @c _subcmd of a command.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_SHELL_SUBCMD_SET_CREATE(_name)                                                 \
  static const struct pbl_shell_cmd _name[1] PBL_USED PBL_UNSORTED_SECTION(pbl_shset) = {{ \
    .help = #_name,                                                                        \
  }}
#else
#define PBL_SHELL_SUBCMD_SET_CREATE(_name)                              \
  static const struct pbl_shell_cmd _name[0] PBL_USED PBL_ALIGNED(4)    \
      PBL_SECTION(".pbl_shell_subcmds." #_name ".!");                   \
  static const struct pbl_shell_cmd _name##_end PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_shell_subcmds." #_name ".~") = PBL_SHELL_SUBCMD_SET_END
#endif

/** @cond INTERNAL_HIDDEN */
#ifdef PBL_NO_LINKER_SCRIPT
/* A set's entries, scattered in their section, name the set they belong to. */
struct pbl_shell_subcmd_entry {
  const char *set;
  struct pbl_shell_cmd cmd;
};

#define PBL_SHELL_SUBCMD_ADD_IMPL(_id, _set, _syntax, _subcmd, _help, _handler, _mand, _opt)       \
  static const struct pbl_shell_subcmd_entry pbl_shell_subcmd_##_id PBL_USED PBL_UNSORTED_SECTION( \
      pbl_shsub) = {                                                                               \
    .set = #_set,                                                                                  \
    .cmd = PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt),                      \
  }
#else
#define PBL_SHELL_SUBCMD_ADD_IMPL(_id, _set, _syntax, _subcmd, _help, _handler, _mand, _opt) \
  static const struct pbl_shell_cmd pbl_shell_subcmd_##_id PBL_USED PBL_ALIGNED(4)           \
      PBL_SECTION(".pbl_shell_subcmds." #_set "." #_syntax) =                                \
          PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt)
#endif

#define PBL_SHELL_SUBCMD_ADD_ID(_id, ...) PBL_SHELL_SUBCMD_ADD_IMPL(_id, __VA_ARGS__)
/** @endcond */

/**
 * @brief Add a subcommand to a set, from any file.
 *
 * Entries of a set that is not built are unreachable.
 *
 * @param _set Set created with PBL_SHELL_SUBCMD_SET_CREATE().
 * @param _syntax Name, an identifier.
 * @param _subcmd Subcommands, or NULL.
 * @param _help Help string, or NULL.
 * @param _handler Handler, or NULL.
 * @param _mand Mandatory arguments, the command name included; 0 skips the check.
 * @param _opt Optional arguments, or @ref PBL_SHELL_OPT_ARG_MAX.
 */
#define PBL_SHELL_SUBCMD_ADD(_set, _syntax, _subcmd, _help, _handler, _mand, _opt) \
  PBL_SHELL_SUBCMD_ADD_ID(__COUNTER__, _set, _syntax, _subcmd, _help, _handler, _mand, _opt)

/**
 * @brief Register a root command with an argument count check.
 *
 * Root commands are sorted by name by the linker script, or when looked up in builds without
 * one.
 *
 * @param _syntax Name, an identifier.
 * @param _subcmd Subcommands, or NULL.
 * @param _help Help string, or NULL.
 * @param _handler Handler, or NULL.
 * @param _mand Mandatory arguments, the command name included; 0 skips the check.
 * @param _opt Optional arguments, or @ref PBL_SHELL_OPT_ARG_MAX.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_SHELL_CMD_ARG_REGISTER(_syntax, _subcmd, _help, _handler, _mand, _opt)              \
  static const struct pbl_shell_cmd pbl_shell_root_cmd_##_syntax PBL_USED PBL_UNSORTED_SECTION( \
      pbl_shroot) = PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt)
#else
#define PBL_SHELL_CMD_ARG_REGISTER(_syntax, _subcmd, _help, _handler, _mand, _opt)       \
  static const struct pbl_shell_cmd pbl_shell_root_cmd_##_syntax PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_shell_root_cmds." #_syntax) =                                    \
          PBL_SHELL_CMD_ARG(_syntax, _subcmd, _help, _handler, _mand, _opt)
#endif

/**
 * @brief Register a root command without an argument count check.
 *
 * @param _syntax Name, an identifier.
 * @param _subcmd Subcommands, or NULL.
 * @param _help Help string, or NULL.
 * @param _handler Handler, or NULL.
 */
#define PBL_SHELL_CMD_REGISTER(_syntax, _subcmd, _help, _handler) \
  PBL_SHELL_CMD_ARG_REGISTER(_syntax, _subcmd, _help, _handler, 0, 0)

/**
 * @brief Print a formatted line.
 *
 * @param sh Shell.
 * @param fmt printf-style format.
 * @param ... Format arguments.
 */
void pbl_shell_print(const struct pbl_shell *sh, const char *fmt, ...) PBL_FORMAT_PRINTF(2, 3);

/**
 * @brief Print formatted text without a line break.
 *
 * Output is truncated to @c CONFIG_SHELL_PRINTF_BUFF_SIZE - 1 characters.
 *
 * @param sh Shell.
 * @param fmt printf-style format.
 * @param ... Format arguments.
 */
void pbl_shell_fprintf(const struct pbl_shell *sh, const char *fmt, ...) PBL_FORMAT_PRINTF(2, 3);

/**
 * @brief Print formatted text without a line break, with a va_list.
 *
 * @param sh Shell.
 * @param fmt printf-style format.
 * @param args Format arguments.
 */
void pbl_shell_vfprintf(const struct pbl_shell *sh, const char *fmt, va_list args);

/**
 * @brief Print a formatted line prefixed with "error: ".
 *
 * @param sh Shell.
 * @param fmt printf-style format.
 * @param ... Format arguments.
 */
void pbl_shell_error(const struct pbl_shell *sh, const char *fmt, ...) PBL_FORMAT_PRINTF(2, 3);

/**
 * @brief Print a hex dump, 16 bytes a line with offsets and ASCII.
 *
 * @param sh Shell.
 * @param data Data.
 * @param len Length of @p data in bytes.
 */
void pbl_shell_hexdump(const struct pbl_shell *sh, const void *data, size_t len);

/**
 * @brief Print the help of a command and the list of its subcommands.
 *
 * @param sh Shell.
 * @param cmd Command.
 */
void pbl_shell_help(const struct pbl_shell *sh, const struct pbl_shell_cmd *cmd);

/**
 * @brief Finish a command whose handler returned -EINPROGRESS.
 *
 * May be called from any thread.
 *
 * @param sh Shell the command runs on.
 * @param ret Result of the command, 0 or a negative errno.
 */
void pbl_shell_cmd_done(const struct pbl_shell *sh, int ret);

/**
 * @brief Parse an argument as a signed integer.
 *
 * Accepts decimal, hex (0x prefix) and octal (0 prefix).
 *
 * @param str Argument.
 * @param[out] out Value.
 * @retval 0 Success.
 * @retval -EINVAL @p str is empty, not a number or out of range.
 */
int pbl_shell_strtol(const char *str, long *out);

/**
 * @brief Parse an argument as an unsigned integer.
 *
 * As pbl_shell_strtol(), rejecting negative numbers.
 *
 * @param str Argument.
 * @param[out] out Value.
 * @retval 0 Success.
 * @retval -EINVAL @p str is empty, negative, not a number or out of range.
 */
int pbl_shell_strtoul(const char *str, unsigned long *out);

/** @} */
