/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/shell/backend.h>
#include <pbl/shell/shell.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pbl/services/system_task.h"

#define ESC_NONE  0
#define ESC_START 1
#define ESC_CSI   2

static const struct pbl_shell_cmd *prv_array_get(const struct pbl_shell_cmd *cmds, size_t idx) {
  return (cmds[idx].syntax != NULL) ? &cmds[idx] : NULL;
}

#ifndef PBL_NO_LINKER_SCRIPT
// The linker script gathers the root commands sorted by name, and the entries of each set
// between the set's two markers.

extern const struct pbl_shell_cmd __pbl_shell_root_cmds_start[];
extern const struct pbl_shell_cmd __pbl_shell_root_cmds_end[];

static const struct pbl_shell_cmd *prv_root_get(size_t idx) {
  const struct pbl_shell_cmd *cmd = &__pbl_shell_root_cmds_start[idx];
  return (cmd < __pbl_shell_root_cmds_end) ? cmd : NULL;
}

static const struct pbl_shell_cmd *prv_subcmd_get(const struct pbl_shell_cmd *subcmd, size_t idx) {
  return prv_array_get(subcmd, idx);
}
#else
// Without the linker script the commands are gathered unsorted, and the entries of a set apart
// from each other, each naming its set: pick them by name when looked up.

extern const struct pbl_shell_cmd __pbl_shell_root_cmds_start[] PBL_UNSORTED_SECTION_START(
    pbl_shroot);
extern const struct pbl_shell_cmd __pbl_shell_root_cmds_end[] PBL_UNSORTED_SECTION_END(pbl_shroot);
extern const struct pbl_shell_cmd __pbl_shell_sets_start[] PBL_UNSORTED_SECTION_START(pbl_shset);
extern const struct pbl_shell_cmd __pbl_shell_sets_end[] PBL_UNSORTED_SECTION_END(pbl_shset);
extern const struct pbl_shell_subcmd_entry __pbl_shell_subcmds_start[] PBL_UNSORTED_SECTION_START(
    pbl_shsub);
extern const struct pbl_shell_subcmd_entry __pbl_shell_subcmds_end[] PBL_UNSORTED_SECTION_END(
    pbl_shsub);

// The idx-th command of a set, or of the root commands if @p set is NULL, in no order.
static const struct pbl_shell_cmd *prv_unsorted_get(const char *set, size_t idx) {
  if (set == NULL) {
    return (&__pbl_shell_root_cmds_start[idx] < __pbl_shell_root_cmds_end)
               ? &__pbl_shell_root_cmds_start[idx]
               : NULL;
  }
  for (const struct pbl_shell_subcmd_entry *e = __pbl_shell_subcmds_start;
       e < __pbl_shell_subcmds_end; e++) {
    if (strcmp(e->set, set) == 0 && idx-- == 0) {
      return &e->cmd;
    }
  }
  return NULL;
}

static const struct pbl_shell_cmd *prv_sorted_get(const char *set, size_t idx) {
  const struct pbl_shell_cmd *prev = NULL;

  for (size_t n = 0; n <= idx; n++) {
    const struct pbl_shell_cmd *next = NULL;
    const struct pbl_shell_cmd *cmd;
    for (size_t i = 0; (cmd = prv_unsorted_get(set, i)) != NULL; i++) {
      if ((prev == NULL || strcmp(cmd->syntax, prev->syntax) > 0) &&
          (next == NULL || strcmp(cmd->syntax, next->syntax) < 0)) {
        next = cmd;
      }
    }
    if (next == NULL) {
      return NULL;
    }
    prev = next;
  }

  return prev;
}

static const struct pbl_shell_cmd *prv_root_get(size_t idx) {
  return prv_sorted_get(NULL, idx);
}

static const struct pbl_shell_cmd *prv_subcmd_get(const struct pbl_shell_cmd *subcmd, size_t idx) {
  if (subcmd >= __pbl_shell_sets_start && subcmd < __pbl_shell_sets_end) {
    return prv_sorted_get(subcmd->help, idx);
  }
  return prv_array_get(subcmd, idx);
}
#endif

static const struct pbl_shell_cmd *prv_level_get(const struct pbl_shell_cmd *parent, size_t idx) {
  if (parent == NULL) {
    return prv_root_get(idx);
  }
  if (parent->subcmd == NULL) {
    return NULL;
  }
  return prv_subcmd_get(parent->subcmd, idx);
}

static const struct pbl_shell_cmd *prv_find(const struct pbl_shell_cmd *parent, const char *name) {
  const struct pbl_shell_cmd *cmd;

  for (size_t i = 0; (cmd = prv_level_get(parent, i)) != NULL; i++) {
    if (strcmp(cmd->syntax, name) == 0) {
      return cmd;
    }
  }

  return NULL;
}

static void prv_write(const struct pbl_shell *sh, const char *data, size_t len) {
  sh->api->write(sh, data, len);
}

static void prv_puts(const struct pbl_shell *sh, const char *str) {
  prv_write(sh, str, strlen(str));
}

void pbl_shell_vfprintf(const struct pbl_shell *sh, const char *fmt, va_list args) {
  char buf[CONFIG_SHELL_PRINTF_BUFF_SIZE];
  int len;

  len = vsniprintf(buf, sizeof(buf), fmt, args);
  if (len <= 0) {
    return;
  }

  prv_write(sh, buf, ((size_t)len < sizeof(buf)) ? (size_t)len : sizeof(buf) - 1);
}

void pbl_shell_fprintf(const struct pbl_shell *sh, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  pbl_shell_vfprintf(sh, fmt, args);
  va_end(args);
}

void pbl_shell_print(const struct pbl_shell *sh, const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  pbl_shell_vfprintf(sh, fmt, args);
  va_end(args);
  prv_puts(sh, "\r\n");
}

void pbl_shell_error(const struct pbl_shell *sh, const char *fmt, ...) {
  va_list args;

  prv_puts(sh, "error: ");
  va_start(args, fmt);
  pbl_shell_vfprintf(sh, fmt, args);
  va_end(args);
  prv_puts(sh, "\r\n");
}

void pbl_shell_hexdump(const struct pbl_shell *sh, const void *data, size_t len) {
  const uint8_t *bytes = data;

  for (size_t off = 0; off < len; off += 16) {
    char ascii[17];
    size_t n = ((len - off) < 16) ? (len - off) : 16;

    pbl_shell_fprintf(sh, "%08x:", (unsigned int)off);
    for (size_t i = 0; i < 16; i++) {
      if (i < n) {
        pbl_shell_fprintf(sh, " %02x", bytes[off + i]);
        ascii[i] = (bytes[off + i] >= 0x20 && bytes[off + i] < 0x7f) ? bytes[off + i] : '.';
      } else {
        prv_puts(sh, "   ");
        ascii[i] = ' ';
      }
    }
    ascii[16] = '\0';
    pbl_shell_print(sh, "  %s", ascii);
  }
}

static void prv_print_level(const struct pbl_shell *sh, const struct pbl_shell_cmd *parent) {
  const struct pbl_shell_cmd *cmd;
  int width = 0;

  for (size_t i = 0; (cmd = prv_level_get(parent, i)) != NULL; i++) {
    int len = strlen(cmd->syntax);
    if (len > width) {
      width = len;
    }
  }

  for (size_t i = 0; (cmd = prv_level_get(parent, i)) != NULL; i++) {
    pbl_shell_print(sh, "  %-*s  %s", width, cmd->syntax, cmd->help ? cmd->help : "");
  }
}

void pbl_shell_help(const struct pbl_shell *sh, const struct pbl_shell_cmd *cmd) {
  if (cmd->help != NULL) {
    pbl_shell_print(sh, "%s - %s", cmd->syntax, cmd->help);
  } else {
    pbl_shell_print(sh, "%s", cmd->syntax);
  }

  if (cmd->subcmd != NULL) {
    pbl_shell_print(sh, "Subcommands:");
    prv_print_level(sh, cmd);
  }
}

static size_t prv_tokenize(char *line, char **argv, size_t max) {
  char *r = line;
  size_t argc = 0;

  while (*r != '\0') {
    char *w;
    bool quoted = false;

    while (*r == ' ' || *r == '\t') {
      r++;
    }
    if (*r == '\0') {
      break;
    }
    if (argc == max) {
      return max + 1;
    }

    argv[argc++] = w = r;
    while (*r != '\0' && (quoted || (*r != ' ' && *r != '\t'))) {
      if (*r == '"') {
        quoted = !quoted;
        r++;
        continue;
      }
      *w++ = *r++;
    }
    if (*r != '\0') {
      r++;
    }
    *w = '\0';
  }

  return argc;
}

static int prv_execute(const struct pbl_shell *sh) {
  struct pbl_shell_ctx *ctx = sh->ctx;
  const struct pbl_shell_cmd *cmd;
  char *argv[CONFIG_SHELL_ARGC_MAX + 1];
  char **args = argv;
  size_t argc;

  ctx->line[ctx->len] = '\0';
  argc = prv_tokenize(ctx->line, argv, CONFIG_SHELL_ARGC_MAX);
  if (argc == 0) {
    return 0;
  }
  if (argc > CONFIG_SHELL_ARGC_MAX) {
    pbl_shell_error(sh, "too many arguments");
    return -E2BIG;
  }

  cmd = prv_find(NULL, argv[0]);
  if (cmd == NULL) {
    pbl_shell_error(sh, "%s: command not found, try 'help'", argv[0]);
    return -ENOEXEC;
  }

  while (argc > 1 && cmd->subcmd != NULL) {
    const struct pbl_shell_cmd *sub = prv_find(cmd, args[1]);
    if (sub == NULL) {
      break;
    }
    cmd = sub;
    args++;
    argc--;
  }

  if (argc > 1 && (strcmp(args[1], "-h") == 0 || strcmp(args[1], "--help") == 0)) {
    pbl_shell_help(sh, cmd);
    return 0;
  }

  if (cmd->handler == NULL) {
    if (argc > 1) {
      pbl_shell_error(sh, "%s: unknown subcommand '%s'", args[0], args[1]);
    }
    pbl_shell_help(sh, cmd);
    return (argc > 1) ? -EINVAL : 0;
  }

  if (cmd->mandatory != 0 && (argc < cmd->mandatory || (cmd->optional != PBL_SHELL_OPT_ARG_MAX &&
                                                        argc > cmd->mandatory + cmd->optional))) {
    pbl_shell_error(sh, "%s: wrong parameter count", args[0]);
    pbl_shell_help(sh, cmd);
    return -EINVAL;
  }

  args[argc] = NULL;
  return cmd->handler(sh, argc, args);
}

static void prv_print_prompt(const struct pbl_shell *sh) {
  prv_puts(sh, sh->prompt);
}

static void prv_rx_process(void *data);

static void prv_finish(const struct pbl_shell *sh, int ret) {
  struct pbl_shell_ctx *ctx = sh->ctx;

  ctx->len = 0;
  ctx->busy = false;

  if (sh->api->done != NULL) {
    sh->api->done(sh, ret);
  }

  if (sh->prompt != NULL && ctx->active) {
    prv_print_prompt(sh);
    if (ctx->rx_head != ctx->rx_tail && !ctx->rx_scheduled) {
      ctx->rx_scheduled = true;
      if (!system_task_add_callback(prv_rx_process, (void *)sh)) {
        ctx->rx_scheduled = false;
      }
    }
  }
}

static void prv_run(const struct pbl_shell *sh) {
  int ret;

  sh->ctx->busy = true;
  ret = prv_execute(sh);
  if (ret != -EINPROGRESS) {
    prv_finish(sh, ret);
  }
}

void pbl_shell_cmd_done(const struct pbl_shell *sh, int ret) {
  prv_finish(sh, ret);
}

bool pbl_shell_is_busy(const struct pbl_shell *sh) {
  return sh->ctx->busy;
}

static void prv_line_run(void *data) {
  prv_run(data);
}

int pbl_shell_execute_line(const struct pbl_shell *sh, const char *line, size_t len) {
  struct pbl_shell_ctx *ctx = sh->ctx;

  if (ctx->busy) {
    return -EBUSY;
  }
  if (len > CONFIG_SHELL_CMD_BUFF_SIZE) {
    return -ENOSPC;
  }

  ctx->busy = true;
  memcpy(ctx->line, line, len);
  ctx->len = len;

  if (!system_task_add_callback(prv_line_run, (void *)sh)) {
    ctx->busy = false;
    return -ENOMEM;
  }

  return 0;
}

static void prv_append(const struct pbl_shell *sh, const char *str, size_t len) {
  struct pbl_shell_ctx *ctx = sh->ctx;

  for (size_t i = 0; i < len; i++) {
    if (ctx->len >= CONFIG_SHELL_CMD_BUFF_SIZE) {
      prv_puts(sh, "\a");
      return;
    }
    ctx->line[ctx->len++] = str[i];
    prv_write(sh, &str[i], 1);
  }
}

static void prv_complete(const struct pbl_shell *sh) {
  struct pbl_shell_ctx *ctx = sh->ctx;
  const struct pbl_shell_cmd *parent = NULL;
  const struct pbl_shell_cmd *first = NULL;
  const struct pbl_shell_cmd *cmd;
  char tmp[CONFIG_SHELL_CMD_BUFF_SIZE + 1];
  char *argv[CONFIG_SHELL_ARGC_MAX + 1];
  size_t start, argc, plen, common = 0, count = 0;
  const char *prefix;

  start = ctx->len;
  while (start > 0 && ctx->line[start - 1] != ' ') {
    start--;
  }

  memcpy(tmp, ctx->line, start);
  tmp[start] = '\0';
  argc = prv_tokenize(tmp, argv, CONFIG_SHELL_ARGC_MAX);
  if (argc > CONFIG_SHELL_ARGC_MAX) {
    return;
  }

  for (size_t i = 0; i < argc; i++) {
    cmd = prv_find(parent, argv[i]);
    if (cmd == NULL || cmd->subcmd == NULL) {
      return;
    }
    parent = cmd;
  }

  prefix = &ctx->line[start];
  plen = ctx->len - start;

  for (size_t i = 0; (cmd = prv_level_get(parent, i)) != NULL; i++) {
    if (strncmp(cmd->syntax, prefix, plen) != 0) {
      continue;
    }
    if (first == NULL) {
      first = cmd;
      common = strlen(cmd->syntax);
    } else {
      size_t k = plen;
      while (k < common && first->syntax[k] == cmd->syntax[k]) {
        k++;
      }
      common = k;
    }
    count++;
  }

  if (count == 0) {
    prv_puts(sh, "\a");
    return;
  }

  if (count == 1) {
    prv_append(sh, &first->syntax[plen], common - plen);
    prv_append(sh, " ", 1);
    return;
  }

  if (common > plen) {
    prv_append(sh, &first->syntax[plen], common - plen);
    return;
  }

  prv_puts(sh, "\r\n");
  for (size_t i = 0; (cmd = prv_level_get(parent, i)) != NULL; i++) {
    if (strncmp(cmd->syntax, prefix, plen) == 0) {
      pbl_shell_print(sh, "  %s", cmd->syntax);
    }
  }
  prv_print_prompt(sh);
  prv_write(sh, ctx->line, ctx->len);
}

static void prv_handle_char(const struct pbl_shell *sh, char c) {
  struct pbl_shell_ctx *ctx = sh->ctx;

  if (ctx->esc == ESC_START) {
    ctx->esc = (c == '[') ? ESC_CSI : ESC_NONE;
    return;
  }
  if (ctx->esc == ESC_CSI) {
    if (c >= 0x40 && c <= 0x7e) {
      ctx->esc = ESC_NONE;
    }
    return;
  }

  if (c == '\n' && ctx->last_cr) {
    ctx->last_cr = false;
    return;
  }
  ctx->last_cr = (c == '\r');

  switch (c) {
    case '\r':
    case '\n':
      prv_puts(sh, "\r\n");
      prv_run(sh);
      break;
    case 0x03:
      prv_puts(sh, "^C\r\n");
      ctx->len = 0;
      prv_print_prompt(sh);
      break;
    case 0x08:
    case 0x7f:
      if (ctx->len > 0) {
        ctx->len--;
        prv_puts(sh, "\b \b");
      } else {
        prv_puts(sh, "\a");
      }
      break;
    case '\t':
      prv_complete(sh);
      break;
    case 0x1b:
      ctx->esc = ESC_START;
      break;
    default:
      if (c >= 0x20 && c < 0x7f) {
        prv_append(sh, &c, 1);
      }
      break;
  }
}

static void prv_rx_process(void *data) {
  const struct pbl_shell *sh = data;
  struct pbl_shell_ctx *ctx = sh->ctx;

  ctx->rx_scheduled = false;

  if (!ctx->active) {
    ctx->active = true;
    ctx->len = 0;
    ctx->esc = ESC_NONE;
    prv_puts(sh, "\r\n");
    prv_print_prompt(sh);
  }

  while (!ctx->busy && ctx->rx_tail != ctx->rx_head) {
    char c = ctx->rx[ctx->rx_tail];
    ctx->rx_tail = (ctx->rx_tail + 1) % CONFIG_SHELL_RX_BUFF_SIZE;
    prv_handle_char(sh, c);
  }
}

static void prv_schedule_from_isr(const struct pbl_shell *sh) {
  struct pbl_shell_ctx *ctx = sh->ctx;

  if (ctx->rx_scheduled) {
    return;
  }

  ctx->rx_scheduled = true;
  if (!system_task_add_callback_from_isr(prv_rx_process, (void *)sh)) {
    ctx->rx_scheduled = false;
  }
}

void pbl_shell_start_from_isr(const struct pbl_shell *sh) {
  sh->ctx->rx_tail = sh->ctx->rx_head;
  sh->ctx->active = false;
  prv_schedule_from_isr(sh);
}

void pbl_shell_stop(const struct pbl_shell *sh) {
  sh->ctx->active = false;
  sh->ctx->rx_tail = sh->ctx->rx_head;
}

void pbl_shell_input_from_isr(const struct pbl_shell *sh, char c) {
  struct pbl_shell_ctx *ctx = sh->ctx;
  uint8_t next = (ctx->rx_head + 1) % CONFIG_SHELL_RX_BUFF_SIZE;

  if (next == ctx->rx_tail) {
    return;
  }

  ctx->rx[ctx->rx_head] = c;
  ctx->rx_head = next;
  prv_schedule_from_isr(sh);
}

static int prv_strto(const char *str, bool is_signed, long *sout, unsigned long *uout) {
  char *end;

  if (str == NULL || *str == '\0') {
    return -EINVAL;
  }

  errno = 0;
  if (is_signed) {
    *sout = strtol(str, &end, 0);
  } else {
    if (*str == '-') {
      return -EINVAL;
    }
    *uout = strtoul(str, &end, 0);
  }

  return (errno != 0 || *end != '\0') ? -EINVAL : 0;
}

int pbl_shell_strtol(const char *str, long *out) {
  return prv_strto(str, true, out, NULL);
}

int pbl_shell_strtoul(const char *str, unsigned long *out) {
  return prv_strto(str, false, NULL, out);
}

static int prv_cmd_help(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "Commands:");
  prv_print_level(sh, NULL);
  pbl_shell_print(sh, "Run '<command> -h' for details.");
  return 0;
}

PBL_SHELL_CMD_REGISTER(help, NULL, "List commands", prv_cmd_help);
